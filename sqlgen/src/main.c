#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#if defined(WIN32)
#else
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

static int
print_error(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    return -1;
}

struct mfile
{
    void* address;
    int size;
};

static int
mfile_map(struct mfile* mf, const char* file_name)
{
#if defined(WIN32)
#else
    struct stat stbuf;
    int fd;

    fd = open(file_name, O_RDONLY);
    if (fd < 0)
        goto open_failed;

    if (fstat(fd, &stbuf) != 0)
        goto fstat_failed;
    if (!S_ISREG(stbuf.st_mode))
        goto fstat_failed;
    mf->address = mmap(NULL, (size_t)stbuf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
    if (mf->address == NULL)
        goto mmap_failed;

    /* file descriptor no longer required */
    close(fd);

    mf->size = (int)stbuf.st_size;
    return 0;

    mmap_failed    :
    fstat_failed   : close(fd);
    open_failed    : return -1;
#endif
}

void
mfile_unmap(struct mfile* mf)
{
#if defined(WIN32)
#else
    munmap(mf->address, (size_t)mf->size);
#endif
}

enum backend
{
    BACKEND_SQLITE3 = 0x01
};

struct cfg
{
    const char* input_file;
    const char* output_header;
    const char* output_source;
    enum backend backends;
};

static int
parse_cmdline(int argc, char** argv, struct cfg* cfg)
{
    int i;
    for (i = 1; i != argc; ++i)
    {
        if (strcmp(argv[i], "-i") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: Missing argument to option -i\n");
                return -1;
            }

            cfg->input_file = argv[++i];
        }
        else if (strcmp(argv[i], "--header") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: Missing argument to option --header\n");
                return -1;
            }

            cfg->output_header = argv[++i];
        }
        else if (strcmp(argv[i], "--source") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: Missing argument to option --source\n");
                return -1;
            }

            cfg->output_source = argv[++i];
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            char* backend;
            char* p;
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: Missing argument to option -b\n");
                return -1;
            }

            backend = argv[++i];
            p = backend;
            while (1)
            {
                while (*p && *p != ',')
                    ++p;

                if (memcmp(backend, "sqlite", sizeof("sqlite") - 1) == 0)
                    cfg->backends |= BACKEND_SQLITE3;
                else
                {
                    *p = 0;
                    fprintf(stderr, "Error: Unknown backend \"%s\"\n", backend);
                    return -1;
                }

                if (!*p)
                    break;

                ++p;
                if (!*p)
                    break;
                backend = p;
            }
        }
        else
        {
            fprintf(stderr, "Error: Unknown option \"%s\"\n", argv[i]);
            return -1;
        }
    }

    if (cfg->backends == 0)
        return print_error("Error: No backends were specified. Use -b <backend1,backend2,...>. Supported backends are: sqlite\n");

    if (cfg->output_header == NULL || !*cfg->output_header)
        return print_error("Error: No output header file was specified. Use --header\n");
    if (cfg->output_source == NULL || !*cfg->output_source)
        return print_error("Error: No output source file was specified. Use --source\n");

    if (cfg->input_file == NULL || !*cfg->input_file)
        return print_error("Error: No input file name was specified. Use -i\n");

    return 0;
}

struct str_view
{
    int off, len;
};

struct parser
{
    const char* data;
    int off;
    int len;
    union {
        struct str_view str;
    } value;
};

enum token
{
    TOK_ERROR = -1,
    TOK_END = 0,
    TOK_OPTION = 256,
    TOK_STRING,
    TOK_LABEL,
    TOK_HEADER_PREAMBLE,
    TOK_HEADER_POSTAMBLE,
    TOK_SOURCE_INCLUDES,
    TOK_SOURCE_PREAMBLE,
    TOK_SOURCE_POSTAMBLE,
    TOK_QUERY,
    TOK_PRIVATE_QUERY,
    TOK_FUNCTION,
    TOK_TYPE,
    TOK_TABLE,
    TOK_STMT,
    TOK_RETURN
};

static enum token
scan_comment_block(struct parser* p)
{
    while (p->off != p->len)
    {
        if (p->data[p->off] == '*' && p->data[p->off+1] == '/')
        {
            p->off += 2;
            return TOK_END;
        }

        p->off++;
    }

    fprintf(stderr, "Error: Missing \"*/\" closing block comment\n");
    return TOK_ERROR;
}

static void
scan_comment_line(struct parser* p)
{
    while (p->off != p->len)
    {
        if (p->data[p->off] == '\n')
        {
            p->off++;
            break;
        }
        p->off++;
    }
}

static enum token
scan_next_token(struct parser* p)
{
    while (p->off != p->len)
    {
        if (p->data[p->off] == '/' && p->data[p->off+1] == '*')
        {
            p->off += 2;
            if (scan_comment_block(p) < 0)
                return TOK_ERROR;
            continue;
        }
        if (p->data[p->off] == '/' && p->data[p->off+1] == '/')
        {
            p->off += 2;
            scan_comment_line(p);
            continue;
        }
        if (isspace(p->data[p->off]) || p->data[p->off] == '\r' || p->data[p->off] == '\n')
        {
            p->off++;
            continue;
        }
        /* ".*?" */
        if (p->data[p->off] == '"')
        {
            p->value.str.off = ++p->off;
            for (; p->off != p->len; ++p->off)
                if (p->data[p->off] == '"')
                    break;
            if (p->off == p->len)
            {
                fprintf(stderr, "Error: Missing closing quote on string\n");
                return TOK_ERROR;
            }
            p->value.str.len = p->off++ - p->value.str.off;
            return TOK_STRING;
        }
        if (memcmp(p->data + p->off, "%option", sizeof("%option") - 1) == 0)
        {
            p->off += sizeof("%option") - 1;
            return TOK_OPTION;
        }
        if (memcmp(p->data + p->off, "%header-preamble", sizeof("%header-preamble") - 1) == 0)
        {
            p->off += sizeof("%header-preamble") - 1;
            return TOK_HEADER_PREAMBLE;
        }
        if (memcmp(p->data + p->off, "%header-postamble", sizeof("%header-postamble") - 1) == 0)
        {
            p->off += sizeof("%header-postamble") - 1;
            return TOK_HEADER_POSTAMBLE;
        }
        if (memcmp(p->data + p->off, "%source-includes", sizeof("%source-includes") - 1) == 0)
        {
            p->off += sizeof("%source-includes") - 1;
            return TOK_SOURCE_INCLUDES;
        }
        if (memcmp(p->data + p->off, "%source-preamble", sizeof("%source-preamble") - 1) == 0)
        {
            p->off += sizeof("%source-preamble") - 1;
            return TOK_SOURCE_PREAMBLE;
        }
        if (memcmp(p->data + p->off, "%source-postamble", sizeof("%source-postamble") - 1) == 0)
        {
            p->off += sizeof("%source-postamble") - 1;
            return TOK_SOURCE_POSTAMBLE;
        }
        if (memcmp(p->data + p->off, "%query", sizeof("%query") - 1) == 0)
        {
            p->off += sizeof("%query") - 1;
            return TOK_QUERY;
        }
        if (memcmp(p->data + p->off, "%private-query", sizeof("%private-query") - 1) == 0)
        {
            p->off += sizeof("%private-query") - 1;
            return TOK_PRIVATE_QUERY;
        }
        if (memcmp(p->data + p->off, "%function", sizeof("%function") - 1) == 0)
        {
            p->off += sizeof("%function") - 1;
            return TOK_FUNCTION;
        }
        if (memcmp(p->data + p->off, "type", sizeof("type") - 1) == 0)
        {
            p->off += sizeof("type") - 1;
            return TOK_TYPE;
        }
        if (memcmp(p->data + p->off, "table", sizeof("table") - 1) == 0)
        {
            p->off += sizeof("table") - 1;
            return TOK_TABLE;
        }
        if (memcmp(p->data + p->off, "return", sizeof("return") - 1) == 0)
        {
            p->off += sizeof("return") - 1;
            return TOK_RETURN;
        }
        if (memcmp(p->data + p->off, "stmt", sizeof("stmt") - 1) == 0)
        {
            p->off += sizeof("stmt") - 1;
            return TOK_STMT;
        }
        if (isalpha(p->data[p->off]))
        {
            p->value.str.off = p->off++;
            while (p->off != p->len && (isalnum(p->data[p->off]) ||
                p->data[p->off] == '-' || p->data[p->off] == '_'))
            {
                p->off++;
            }
            p->value.str.len = p->off - p->value.str.off;
            return TOK_LABEL;
        }

        return p->data[p->off++];
    }

    return TOK_END;
}

struct arg
{
    struct arg* next;
    struct str_view type;
    struct str_view name;
};

static struct arg*
arg_alloc(void)
{
    struct arg* a = malloc(sizeof *a);
    memset(a, 0, sizeof *a);
    return a;
}

enum query_type
{
    QUERY_NONE,
    QUERY_INSERT_OR_IGNORE,
    QUERY_INSERT_OR_GET,
    QUERY_GET_SINGLE,
    QUERY_GET_MULTI,
    QUERY_EXISTS
};

struct query
{
    struct query* next;
    struct str_view name;
    struct str_view stmt;
    struct str_view table_name;
    struct str_view return_name;
    struct arg* args;
    enum query_type type;
};

static struct query*
query_alloc(void)
{
    struct query* q = malloc(sizeof *q);
    memset(q, 0, sizeof *q);
    return q;
}

struct query_group
{
    struct query_group* next;
    struct query* queries;
    struct str_view name;
};

static struct query_group*
query_group_alloc(void)
{
    struct query_group* g = malloc(sizeof *g);
    memset(g, 0, sizeof *g);
    return g;
}

struct function
{
    struct function* next;
    struct str_view name;
    struct str_view body;
    struct arg* args;
};

static struct function*
function_alloc(void)
{
    struct function* f = malloc(sizeof *f);
    memset(f, 0, sizeof *f);
    return f;
}

struct root
{
    struct str_view prefix;
    struct str_view malloc;
    struct str_view free;
    struct str_view header_preamble;
    struct str_view header_postamble;
    struct str_view source_includes;
    struct str_view source_preamble;
    struct str_view source_postamble;
    struct query_group* query_groups;
    struct query* queries;
    struct function* functions;
};

static void
root_init(struct root* root)
{
    memset(root, 0, sizeof *root);
}

static enum token
scan_block(struct parser* p, int expect_opening_brace)
{
    int depth = 1;
    if (expect_opening_brace)
        if (scan_next_token(p) != '{')
            return print_error("Error: Expecting '{'\n");

    /*for (; p->off != p->len; p->off++)
        if (!isspace(p->data[p->off]) && p->data[p->off] != '\r' && p->data[p->off] != '\n')
            break;*/
    if (p->off == p->len)
    {
        fprintf(stderr, "Error: Missing closing \"}\"\n");
        return TOK_ERROR;
    }

    p->value.str.off = p->off;
    while (p->off != p->len)
    {
        if (p->data[p->off] == '{')
            depth++;
        else if (p->data[p->off] == '}')
            if (--depth == 0)
            {
                p->value.str.len = p->off++ - p->value.str.off;
                return TOK_STRING;
            }

        p->off++;
    }

    fprintf(stderr, "Error: Missing closing \"}\"\n");
    return TOK_ERROR;
}

static int
parse(struct parser* p, struct root* root)
{
    do {
        switch (scan_next_token(p)) {
            case TOK_OPTION: {
                struct str_view option;
                if (scan_next_token(p) != TOK_LABEL)
                    return print_error("Error: Expected option name after %%option\n");
                option = p->value.str;

                if (scan_next_token(p) != '=')
                    return print_error("Error: Expecting '='\n");
                if (scan_next_token(p) != TOK_STRING)
                    return print_error("Error: Expected string for %%option\n");

                if (memcmp(p->data + option.off, "prefix", sizeof("prefix") - 1) == 0)
                    root->prefix = p->value.str;
                else if (memcmp(p->data + option.off, "malloc", sizeof("malloc") - 1) == 0)
                    root->malloc = p->value.str;
                else if (memcmp(p->data + option.off, "free", sizeof("free") - 1) == 0)
                    root->free = p->value.str;
                else
                    return print_error("Unknown option \"%.*s\"\n", option.len, p->data + option.off);
            } break;

            case TOK_HEADER_PREAMBLE: {
                if (scan_block(p, 1) != TOK_STRING)
                    return -1;
                root->header_preamble = p->value.str;
            } break;

            case TOK_HEADER_POSTAMBLE: {
                if (scan_block(p, 1) != TOK_STRING)
                    return -1;
                root->header_postamble = p->value.str;
            } break;

            case TOK_SOURCE_INCLUDES: {
                if (scan_block(p, 1) != TOK_STRING)
                    return -1;
                root->source_includes = p->value.str;
            } break;

            case TOK_SOURCE_PREAMBLE: {
                if (scan_block(p, 1) != TOK_STRING)
                    return -1;
                root->source_preamble = p->value.str;
            } break;

            case TOK_SOURCE_POSTAMBLE: {
                if (scan_block(p, 1) != TOK_STRING)
                    return -1;
                root->source_postamble = p->value.str;
            } break;

            case TOK_PRIVATE_QUERY:
            case TOK_QUERY: {
                struct str_view group_name = {0};
                struct query* query = query_alloc();

                /* Parse "group,name" or "name"*/
                if (scan_next_token(p) != TOK_LABEL)
                    return print_error("Error: Expected label or group for %%query\n");
                query->name = p->value.str;
                switch (scan_next_token(p)) {
                    case '(': break;
                    case ',':
                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: Expected label for %%query\n");
                        group_name = query->name;
                        query->name = p->value.str;

                        if (scan_next_token(p) == '(')
                            break;
                        /* fallthrough */
                    default:
                        return print_error("Error: Expected \"(\"\n");
                }

                /* Parse parameter list */
                expect_next_param:
                switch (scan_next_token(p))
                {
                    case ')': break;
                    case ',':
                        if (query->args == NULL)
                            return print_error("Error: Expected parameter after \"(\"\n");
                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: Expected parameter after \",\"\n");
                        /* fallthrough */
                    case TOK_LABEL: {
                        struct arg* arg;

                        arg = arg_alloc();
                        arg->type = p->value.str;
                        /* Special case, struct -> expect another label */
                        if (memcmp(p->data + arg->type.off, "struct", sizeof("struct") - 1) == 0)
                        {
                            if (scan_next_token(p) != TOK_LABEL)
                                return print_error("Error: struct without name\n");
                            arg->type.len = p->value.str.off + p->value.str.len - arg->type.off;
                        }

                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: struct without name\n");
                        arg->name = p->value.str;

                        if (query->args == NULL)
                            query->args = arg;
                        else
                        {
                            struct arg* args = query->args;
                            while (args->next)
                                args = args->next;
                            args->next = arg;
                        }

                        goto expect_next_param;
                    } break;

                    default:
                        return print_error("Error: Expected parameter list\n");
                }

                if (scan_next_token(p) != '{')
                    return print_error("Error: Expected \"{\"\n");

                expect_next_stmt:
                switch (scan_next_token(p)) {
                    case TOK_TYPE: {
                        struct str_view t;
                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: Expected query type after \"type\"\n");
                        t = p->value.str;
                        if (memcmp(p->data + t.off, "insert-or-get", t.len) == 0)
                            query->type = QUERY_INSERT_OR_GET;
                        else if (memcmp(p->data + t.off, "insert-or-ignore", t.len) == 0)
                            query->type = QUERY_INSERT_OR_IGNORE;
                        else if (memcmp(p->data + t.off, "exists", t.len) == 0)
                            query->type = QUERY_EXISTS;
                        else if (memcmp(p->data + t.off, "get-single", t.len) == 0)
                            query->type = QUERY_GET_SINGLE;
                        else if (memcmp(p->data + t.off, "get-multi", t.len) == 0)
                            query->type = QUERY_GET_MULTI;
                        else
                            return print_error("Error: Unknown query type \"%.*s\"\n", t.len, p->data + t.off);
                    } goto expect_next_stmt;

                    case TOK_TABLE: {
                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: Expected query type after \"type\"\n");
                        query->table_name = p->value.str;
                    } goto expect_next_stmt;

                    case TOK_STMT: {
                        switch (scan_next_token(p)) {
                            case TOK_LABEL:
                                query->stmt = p->value.str;
                                break;
                            case '{':
                                if (scan_block(p, 0) != TOK_STRING)
                                    return -1;
                                query->stmt = p->value.str;
                                break;
                            default:
                                return print_error("Error: Expected query statement after \"stmt\"\n");
                        }
                    } goto expect_next_stmt;

                    case TOK_RETURN: {
                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: Expected return value after \"return\"\n");
                        query->return_name = p->value.str;
                    } goto expect_next_stmt;

                    case '}': break;
                    default:
                        return print_error("Error: Expecting \"type\", \"table\", \"stmt\" or \"return\"\n");
                }

                if (group_name.len)
                {
                    struct query_group* g = root->query_groups;
                    while (g)
                    {
                        if (g->name.len == group_name.len &&
                            memcmp(p->data + g->name.off, p->data + group_name.off, g->name.len) == 0)
                        {
                            break;
                        }
                        g = g->next;
                    }
                    if (g == NULL)
                    {
                        g = query_group_alloc();
                        g->name = group_name;
                        if (root->query_groups == NULL)
                            root->query_groups = g;
                        else
                        {
                            struct query_group* node = root->query_groups;
                            while (node->next)
                                node = node->next;
                            node->next = g;
                        }
                    }

                    if (g->queries == NULL)
                        g->queries = query;
                    else
                    {
                        struct query* q = g->queries;
                        while (q->next)
                            q = q->next;
                        q->next = query;
                    }
                }
                else
                {
                    if (root->queries == NULL)
                        root->queries = query;
                    else
                    {
                        struct query* q = root->queries;
                        while (q->next)
                            q = q->next;
                        q->next = query;
                    }
                }
            } break;

            case TOK_FUNCTION: {
                struct function* func = function_alloc();

                /* Parse function name */
                if (scan_next_token(p) != TOK_LABEL)
                    return print_error("Error: Expected label or group for %%query\n");
                func->name = p->value.str;
                if (scan_next_token(p) != '(')
                    return print_error("Error: Expected \"(\"\n");

                /* Parse parameter list */
                expect_next_param_func:
                switch (scan_next_token(p))
                {
                    case ')': break;
                    case ',':
                        if (func->args == NULL)
                            return print_error("Error: Expected parameter after \"(\"\n");
                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: Expected parameter after \",\"\n");
                        /* fallthrough */
                    case TOK_LABEL: {
                        struct arg* arg;

                        arg = arg_alloc();
                        arg->type = p->value.str;
                        /* Special case, struct -> expect another label */
                        if (memcmp(p->data + arg->type.off, "struct", sizeof("struct") - 1) == 0)
                        {
                            if (scan_next_token(p) != TOK_LABEL)
                                return print_error("Error: struct without name\n");
                            arg->type.len = p->value.str.off + p->value.str.len - arg->type.off;
                        }

                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: struct without name\n");
                        arg->name = p->value.str;

                        if (func->args == NULL)
                            func->args = arg;
                        else
                        {
                            struct arg* args = func->args;
                            while (args->next)
                                args = args->next;
                            args->next = arg;
                        }

                        goto expect_next_param_func;
                    } break;

                    default:
                        return print_error("Error: Expected parameter list\n");
                }

                if (scan_block(p, 1) != TOK_STRING)
                    return -1;
                func->body = p->value.str;

                if (root->functions == NULL)
                    root->functions = func;
                else
                {
                    struct function* f = root->functions;
                    while (f->next)
                        f = f->next;
                    f->next = func;
                }
            } break;

            case TOK_END: return 0;
            default: return -1;
        }
    } while(1);
}

static void
post_parse(struct root* root, const char* data)
{

}

static int
gen_header(const struct root* root, const char* data, const char* file_name)
{
    struct query* q;
    struct query_group* g;
    struct function* f;
    struct arg* a;
    FILE* fp = fopen(file_name, "w");
    if (fp == NULL)
        return print_error("Error: Failed to open file for writing \"%s\"\n", file_name);

    if (root->header_preamble.len)
        fprintf(fp, "%.*s\n", root->header_preamble.len, data + root->header_preamble.off);

    fprintf(fp, "struct %.*s;\n", root->prefix.len, data + root->prefix.off);
    fprintf(fp, "struct %.*s_interface\n{\n", root->prefix.len, data + root->prefix.off);

    /* Open and close functions are hard-coded */
    fprintf(fp, "    struct %.*s* (*open)(const char* uri);\n",
            root->prefix.len, data + root->prefix.off);
    fprintf(fp, "    void (*close)(struct %.*s* ctx);\n\n",
            root->prefix.len, data + root->prefix.off);

    /* Functions */
    f = root->functions;
    while (f)
    {
        fprintf(fp, "    int (*%.*s)(struct %.*s* ctx",
                f->name.len, data + f->name.off,
                root->prefix.len, data + root->prefix.off);
        a = f->args;
        while (a)
        {
            fprintf(fp, ", ");
            fprintf(fp, "%.*s %.*s",
                a->type.len, data + a->type.off,
                a->name.len, data + a->name.off);
            a = a->next;
        }
        fprintf(fp, ");\n");

        f = f->next;
    }

    /* Global queries */
    q = root->queries;
    while (q)
    {
        fprintf(fp, "    int (*%.*s)(struct %.*s* ctx",
                q->name.len, data + q->name.off,
                root->prefix.len, data + root->prefix.off);
        a = q->args;
        while (a)
        {
            fprintf(fp, ", ");
            fprintf(fp, "%.*s %.*s",
                a->type.len, data + a->type.off,
                a->name.len, data + a->name.off);
            a = a->next;
        }
        fprintf(fp, ");\n");

        q = q->next;
    }
    fprintf(fp, "\n");

    /* Grouped queries */
    g = root->query_groups;
    while (g)
    {
        fprintf(fp, "    struct {\n");
        q = g->queries;
        while (q)
        {
            fprintf(fp, "        int (*%.*s)(struct %.*s* ctx",
                    q->name.len, data + q->name.off,
                    root->prefix.len, data + root->prefix.off);
            a = q->args;
            while (a)
            {
                fprintf(fp, ", %.*s %.*s",
                    a->type.len, data + a->type.off,
                    a->name.len, data + a->name.off);
                a = a->next;
            }
            fprintf(fp, ");\n");

            q = q->next;
        }
        fprintf(fp, "    } %.*s;\n\n", g->name.len, data + g->name.off);

        g = g->next;
    }
    fprintf(fp, "};\n\n");

    if (root->header_postamble.len)
        fprintf(fp, "%.*s\n", root->header_postamble.len, data + root->header_postamble.off);

    fclose(fp);
    return 0;
}

static int
gen_source(const struct root* root, const char* data, const char* file_name)
{
    int i;
    struct query* q;
    struct query_group* g;
    struct function* f;
    struct arg* a;
    FILE* fp = fopen(file_name, "w");
    if (fp == NULL)
        return print_error("Error: Failed to open file for writing \"%s\"\n", file_name);

    if (root->source_includes.len)
        fprintf(fp, "%.*s\n", root->source_includes.len, data + root->source_includes.off);

    /* ------------------------------------------------------------------------
     * Context structure declaration
     * --------------------------------------------------------------------- */

    fprintf(fp, "struct %.*s\n{\n",
            root->prefix.len, data + root->prefix.off);
    fprintf(fp, "    sqlite3* db;\n");
    /* Global queries */
    q = root->queries;
    while (q)
    {
        fprintf(fp, "    sqlite3_stmt* %.*s;\n", q->name.len, data + q->name.off);
        q = q->next;
    }
    /* Grouped queries */
    g = root->query_groups;
    while (g)
    {
        q = g->queries;
        while (q)
        {
            fprintf(fp, "    sqlite3_stmt* %.*s_%.*s;\n",
                    g->name.len, data + g->name.off,
                    q->name.len, data + q->name.off);
            q = q->next;
        }
        g = g->next;
    }
    fprintf(fp, "};\n\n");

    if (root->source_preamble.len)
        fprintf(fp, "%.*s\n", root->source_preamble.len, data + root->source_preamble.off);

    /* ------------------------------------------------------------------------
     * Functions
     * --------------------------------------------------------------------- */

    f = root->functions;
    while (f)
    {
        fprintf(fp, "static int\n%.*s(struct %.*s* ctx",
                f->name.len, data + f->name.off,
                root->prefix.len, data + root->prefix.off);
        a = f->args;
        while (a)
        {
            fprintf(fp, ", %.*s %.*s",
                a->type.len, data + a->type.off,
                a->name.len, data + a->name.off);
            a = a->next;
        }
        fprintf(fp, ")\n{");
        fprintf(fp, "%.*s", f->body.len, data + f->body.off);
        fprintf(fp, "}\n\n");

        f = f->next;
    }

    /* ------------------------------------------------------------------------
     * Query implementations
     * --------------------------------------------------------------------- */

    g = root->query_groups;
    while (g)
    {
        q = g->queries;
        while (q)
        {
            fprintf(fp, "static int\n%.*s_%.*s(struct %.*s* ctx",
                    g->name.len, data + g->name.off,
                    q->name.len, data + q->name.off,
                    root->prefix.len, data + root->prefix.off);

            a = q->args;
            while (a)
            {
                fprintf(fp, ", %.*s %.*s",
                    a->type.len, data + a->type.off,
                    a->name.len, data + a->name.off);
                a = a->next;
            }
            fprintf(fp, ")\n{\n");

            fprintf(fp, "    int ret");
            if (q->return_name.len)
                fprintf(fp, ", %.*s = -1", q->return_name.len, data + q->return_name.off);
            fprintf(fp, ";\n");

            fprintf(fp, "    if (ctx->%.*s_%.*s == NULL)\n",
                    g->name.len, data + g->name.off,
                    q->name.len, data + q->name.off);
            fprintf(fp, "        if ((ret = sqlite3_prepare_v2(ctx->db,\n");
            if (q->stmt.len)
            {
                int p = 0;
                for (; p != q->stmt.len; ++p)
                    if (!isspace(data[q->stmt.off + p]) && data[q->stmt.off + p] != '\n')
                        break;

                fprintf(fp, "            \"");
                for (; p != q->stmt.len; ++p)
                {
                    if (data[q->stmt.off + p] == '\n')
                    {
                        fprintf(fp, " \"\n");
                        for (; p != q->stmt.len; ++p)
                            if (!isspace(data[q->stmt.off + p + 1]))
                                break;
                        if (p != q->stmt.len)
                            fprintf(fp, "            \"");
                    }
                    else
                        fprintf(fp, "%c", data[q->stmt.off + p]);
                }
                fprintf(fp, "\",\n");
            }
            else
            {
                switch (q->type)
                {
                    case QUERY_INSERT_OR_IGNORE:
                        fprintf(fp, "            \"INSERT OR IGNORE INTO %.*s (", q->table_name.len, data + q->table_name.off);
                        break;
                    case QUERY_INSERT_OR_GET:
                        fprintf(fp, "            \"INSERT INTO %.*s (", q->table_name.len, data + q->table_name.off);
                        break;
                    case QUERY_EXISTS:
                        fprintf(fp, "            \"SELECT 1 FROM %.*s", q->table_name.len, data + q->table_name.off);
                        break;
                }
                switch (q->type)
                {
                    case QUERY_INSERT_OR_IGNORE:
                    case QUERY_INSERT_OR_GET:
                        a = q->args;
                        while (a) {
                            if (a != q->args) fprintf(fp, ", ");
                            fprintf(fp, "%.*s", a->name.len, data + a->name.off);
                            a = a->next;
                        }
                        fprintf(fp, ") VALUES (");
                        a = q->args;
                        while (a) {
                            if (a != q->args) fprintf(fp, ", ");
                            fprintf(fp, "?");
                            a = a->next;
                        }
                        fprintf(fp, ")");
                        break;
                    case QUERY_EXISTS:
                        a = q->args;
                        while (a) {
                            if (a == q->args) fprintf(fp, " \"\n            \"WHERE "); else fprintf(fp, "AND ");
                            fprintf(fp, "%.*s = ?", a->name.len, data + a->name.off);
                            a = a->next;
                        }
                        break;
                }
                switch (q->type)
                {
                    case QUERY_EXISTS:
                    case QUERY_INSERT_OR_IGNORE:
                        fprintf(fp, ";\",\n");
                        break;
                    case QUERY_INSERT_OR_GET:
                        fprintf(fp, " \"\n");
                        fprintf(fp, "            \"ON CONFLICT DO UPDATE SET %.*s=excluded.%.*s RETURNING %.*s;\",\n",
                                q->return_name.len, data + q->return_name.off,
                                q->return_name.len, data + q->return_name.off,
                                q->return_name.len, data + q->return_name.off);
                        break;
                }
            }
            fprintf(fp, "            -1, &ctx->%.*s_%.*s, NULL)) != SQLITE_OK)\n",
                    g->name.len, data + g->name.off,
                    q->name.len, data + q->name.off);
            fprintf(fp, "        {\n");
            fprintf(fp, "            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));\n");
            fprintf(fp, "            return -1;\n");
            fprintf(fp, "        }\n");
            fprintf(fp, "\n");

            /* Bind arguments */
            a = q->args;
            i = 1;
            for (; a; a = a->next, i++)
            {
                const char* sqlite_type = "";
                const char* cast = "";

                if (a == q->args) fprintf(fp, "    if ((ret = ");
                else              fprintf(fp, "        (ret = ");

                if (memcmp(data + a->type.off, "uint64_t", sizeof("uint64_t") - 1) == 0)
                    { sqlite_type = "int64"; cast = "(int64_t)"; }
                else if (memcmp(data + a->type.off, "int64_t", sizeof("int64_t") - 1) == 0)
                    { sqlite_type = "int64"; }
                if (memcmp(data + a->type.off, "int", sizeof("int") - 1) == 0)
                    { sqlite_type = "int"; }
                else if (memcmp(data + a->type.off, "struct str_view", sizeof("struct str_view") - 1) == 0)
                    { sqlite_type = "text"; }

                fprintf(fp, "sqlite3_bind_%s(ctx->%.*s_%.*s, %d, %s%.*s",
                        sqlite_type,
                        g->name.len, data + g->name.off,
                        q->name.len, data + q->name.off,
                        i, cast,
                        a->name.len, data + a->name.off);

                if (memcmp(data + a->type.off, "struct str_view", sizeof("struct str_view") - 1) == 0)
                    fprintf(fp, ".data, %.*s.len, SQLITE_STATIC", a->name.len, data + a->name.off);
                fprintf(fp, ")) != SQLITE_OK");

                if (a->next)
                    fprintf(fp, " ||\n");
            }
            fprintf(fp, ")\n    {\n");
            fprintf(fp, "        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));\n");
            fprintf(fp, "        return -1;\n    }\n\n");

            /* Execute statement */
            switch (q->type)
            {
                case QUERY_EXISTS:
                    fprintf(fp, "next_step:\n");
                    fprintf(fp, "    ret = sqlite3_step(ctx->%.*s_%.*s);\n",
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "    switch (ret)\n    {\n");
                    fprintf(fp, "        case SQLITE_ROW:\n");
                    fprintf(fp, "            sqlite3_reset(ctx->%.*s_%.*s);\n",
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "            return 1;\n");
                    fprintf(fp, "        case SQLITE_BUSY: goto next_step;\n");
                    fprintf(fp, "        case SQLITE_DONE:\n");
                    fprintf(fp, "            sqlite3_reset(ctx->%.*s_%.*s);\n",
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "            return 0;\n");
                    fprintf(fp, "    }\n\n");
                    fprintf(fp, "    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));\n");
                    fprintf(fp, "    sqlite3_reset(ctx->%.*s_%.*s);\n",
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "    return -1;\n");
                    break;
                case QUERY_INSERT_OR_IGNORE:
                    fprintf(fp, "next_step:\n");
                    fprintf(fp, "    ret = sqlite3_step(ctx->%.*s_%.*s);\n",
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "    switch (ret)\n    {\n");
                    fprintf(fp, "        case SQLITE_BUSY: goto next_step;\n");
                    fprintf(fp, "        case SQLITE_DONE:\n");
                    fprintf(fp, "            sqlite3_reset(ctx->%.*s_%.*s);\n",
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "            return 0;\n");
                    fprintf(fp, "    }\n\n");
                    fprintf(fp, "    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));\n");
                    fprintf(fp, "    sqlite3_reset(ctx->%.*s_%.*s);\n",
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "    return -1;\n");
                    break;
                case QUERY_INSERT_OR_GET: {
                    fprintf(fp, "next_step:\n");
                    fprintf(fp, "    ret = sqlite3_step(ctx->%.*s_%.*s);\n",
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "    switch (ret)\n    {\n");
                    fprintf(fp, "        case SQLITE_ROW:\n");
                    fprintf(fp, "            %.*s = sqlite3_column_int(ctx->%.*s_%.*s, 0);\n",
                            q->return_name.len, data + q->return_name.off,
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "            goto done;\n");
                    fprintf(fp, "        case SQLITE_BUSY: goto next_step;\n");
                    fprintf(fp, "        case SQLITE_DONE: goto done;\n");
                    fprintf(fp, "    }\n\n");
                    fprintf(fp, "    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));\n");
                    fprintf(fp, "done:\n");
                    fprintf(fp, "    sqlite3_reset(ctx->%.*s_%.*s);\n",
                            g->name.len, data + g->name.off,
                            q->name.len, data + q->name.off);
                    fprintf(fp, "    return %.*s;\n", q->return_name.len, data + q->return_name.off);
                } break;
            }

            fprintf(fp, "}\n\n");

            q = q->next;
        }
        g = g->next;
    }

    /* ------------------------------------------------------------------------
     * Open and close
     * --------------------------------------------------------------------- */

    fprintf(fp, "static struct %.*s*\n%.*s_open(const char* uri)\n{\n",
            root->prefix.len, data + root->prefix.off,
            root->prefix.len, data + root->prefix.off);
    fprintf(fp, "    int ret;\n");
    fprintf(fp, "    struct %.*s* ctx = %.*s(sizeof *ctx);\n",
            root->prefix.len, data + root->prefix.off,
            root->malloc.len ? root->malloc.len : 6, root->malloc.len ? data + root->malloc.off : "malloc");
    fprintf(fp, "    if (ctx == NULL)\n");
    fprintf(fp, "        return NULL;\n");
    fprintf(fp, "    memset(ctx, 0, sizeof *ctx);\n\n");
    fprintf(fp, "    ret = sqlite3_open_v2(uri, &ctx->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);\n");
    fprintf(fp, "    if (ret == SQLITE_OK)\n");
    fprintf(fp, "        return ctx;\n\n");
    fprintf(fp, "    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));\n");
    fprintf(fp, "    %.*s(ctx);\n",
            root->free.len ? root->free.len : 6, root->free.len ? data + root->free.off : "free");
    fprintf(fp, "    return NULL;\n");
    fprintf(fp, "}\n\n");

    fprintf(fp, "static void\n%.*s_close(struct %.*s* ctx)\n{\n",
            root->prefix.len, data + root->prefix.off,
            root->prefix.len, data + root->prefix.off);
    /* Global queries */
    q = root->queries;
    while (q)
    {
        fprintf(fp, "    sqlite3_finalize(ctx->%.*s);\n", q->name.len, data + q->name.off);
        q = q->next;
    }
    /* Grouped queries */
    g = root->query_groups;
    while (g)
    {
        q = g->queries;
        while (q)
        {
            fprintf(fp, "    sqlite3_finalize(ctx->%.*s_%.*s);\n",
                    g->name.len, data + g->name.off,
                    q->name.len, data + q->name.off);
            q = q->next;
        }
        g = g->next;
    }
    fprintf(fp, "    sqlite3_close(ctx->db);\n");
    fprintf(fp, "    %.*s(ctx);\n",
            root->free.len ? root->free.len : 6, root->free.len ? data + root->free.off : "free");
    fprintf(fp, "}\n\n");

    /* ------------------------------------------------------------------------
     * Interface
     * --------------------------------------------------------------------- */

    fprintf(fp, "static struct %.*s_interface db_sqlite = {\n", root->prefix.len, data + root->prefix.off);
    fprintf(fp, "    %.*s_open,\n    %.*s_close,\n",
            root->prefix.len, data + root->prefix.off,
            root->prefix.len, data + root->prefix.off);
    /* Functions */
    f = root->functions;
    while (f)
    {
        fprintf(fp, "    %.*s,\n", f->name.len, data + f->name.off);
        f = f->next;
    }
    /* Global queries */
    q = root->queries;
    while (q)
    {
        fprintf(fp, "    %.*s,\n", q->name.len, data + q->name.off);
        q = q->next;
    }
    /* Grouped queries */
    g = root->query_groups;
    while (g)
    {
        fprintf(fp, "    {\n");
        q = g->queries;
        while (q)
        {
            fprintf(fp, "        %.*s_%.*s,\n",
                    g->name.len, data + g->name.off,
                    q->name.len, data + q->name.off);
            q = q->next;
        }
        fprintf(fp, "    },\n");
        g = g->next;
    }
    fprintf(fp, "};\n\n");

    if (root->source_postamble.len)
        fprintf(fp, "%.*s\n", root->source_postamble.len, data + root->source_postamble.off);

    fclose(fp);
    return 0;
}

int main(int argc, char** argv)
{
    struct parser parser;
    struct mfile mf;
    struct root root;
    struct cfg cfg = {0};
    if (parse_cmdline(argc, argv, &cfg) != 0)
        return -1;

    if (mfile_map(&mf, cfg.input_file) < 0)
        return -1;

    root_init(&root);
    parser.data = (char*)mf.address;
    parser.off = 0;
    parser.len = mf.size;
    parse(&parser, &root);
    if (parse(&parser, &root) != 0)
        return -1;

    post_parse(&root, mf.address);

    if (gen_header(&root, mf.address, cfg.output_header) < 0)
        return -1;
    if (gen_source(&root, mf.address, cfg.output_source) < 0)
        return -1;

    return 0;
}
