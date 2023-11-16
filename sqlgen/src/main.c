#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#if defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define NL "\r\n"
#else
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#define NL "\n"
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

#if defined(WIN32)
wchar_t*
utf8_to_utf16(const char* utf8, int utf8_bytes)
{
    int utf16_bytes = MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, NULL, 0);
    if (utf16_bytes == 0)
        return NULL;

    wchar_t* utf16 = malloc((sizeof(wchar_t) + 1) * utf16_bytes);
    if (utf16 == NULL)
        return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, utf16, utf16_bytes) == 0)
    {
        free(utf16);
        return NULL;
    }

    utf16[utf16_bytes] = 0;

    return utf16;
}
void
utf_free(void* utf)
{
    free(utf);
}
#endif

static int
mfile_map(struct mfile* mf, const char* file_name)
{
#if defined(WIN32)
    HANDLE hFile;
    LARGE_INTEGER liFileSize;
    HANDLE mapping;
    wchar_t* utf16_filename;

    utf16_filename = utf8_to_utf16(file_name, (int)strlen(file_name));
    if (utf16_filename == NULL)
        goto utf16_conv_failed;

    /* Try to open the file */
    hFile = CreateFileW(
        utf16_filename,         /* File name */
        GENERIC_READ,           /* Read only */
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,                   /* Default security */
        OPEN_EXISTING,          /* File must exist */
        FILE_ATTRIBUTE_NORMAL,  /* Default attributes */
        NULL);                  /* No attribute template */
    if (hFile == INVALID_HANDLE_VALUE)
        goto open_failed;

    /* Determine file size in bytes */
    if (!GetFileSizeEx(hFile, &liFileSize))
        goto get_file_size_failed;
    if (liFileSize.QuadPart > (1ULL << 32) - 1)  /* mf->size is an int */
        goto get_file_size_failed;

    mapping = CreateFileMapping(
        hFile,                 /* File handle */
        NULL,                  /* Default security attributes */
        PAGE_READONLY,         /* Read only (or copy on write, but we don't write) */
        0, 0,                  /* High/Low size of mapping. Zero means entire file */
        NULL);                 /* Don't name the mapping */
    if (mapping == NULL)
        goto create_file_mapping_failed;

    mf->address = MapViewOfFile(
        mapping,               /* File mapping handle */
        FILE_MAP_READ,         /* Read-only view of file */
        0, 0,                  /* High/Low offset of where the mapping should begin in the file */
        0);                    /* Length of mapping. Zero means entire file */
    if (mf->address == NULL)
        goto map_view_failed;

    /* The file mapping isn't required anymore */
    CloseHandle(mapping);
    CloseHandle(hFile);
    utf_free(utf16_filename);

    mf->size = liFileSize.QuadPart;

    return 0;

    map_view_failed            : CloseHandle(mapping);
    create_file_mapping_failed :
    get_file_size_failed       : CloseHandle(hFile);
    open_failed                : utf_free(utf16_filename);
    utf16_conv_failed          : return -1;
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
    mf->address = mmap(NULL, (size_t)stbuf.st_size, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
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
    UnmapViewOfFile(mf->address);
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

static int
str_view_eq(const char* s1, struct str_view s2, const char* data)
{
    int len = strlen(s1);
    return len == s2.len && memcmp(data + s2.off, s1, len) == 0;
}

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
    TOK_CALLBACK,
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
        if (memcmp(p->data + p->off, "stmt", sizeof("stmt") - 1) == 0)
        {
            p->off += sizeof("stmt") - 1;
            return TOK_STMT;
        }
        if (memcmp(p->data + p->off, "callback", sizeof("callback") - 1) == 0)
        {
            p->off += sizeof("callback") - 1;
            return TOK_CALLBACK;
        }
        if (memcmp(p->data + p->off, "return", sizeof("return") - 1) == 0)
        {
            p->off += sizeof("return") - 1;
            return TOK_RETURN;
        }
        if (isalpha(p->data[p->off]))
        {
            p->value.str.off = p->off++;
            while (p->off != p->len && (isalnum(p->data[p->off]) ||
                p->data[p->off] == '-' || p->data[p->off] == '_' || p->data[p->off] == '*'))
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
    char nullable;
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
    QUERY_INSERT,
    QUERY_EXISTS,
    QUERY_SELECT_FIRST,
    QUERY_SELECT_ALL,
};

struct query
{
    struct query* next;
    struct str_view name;
    struct str_view stmt;
    struct str_view table_name;
    struct str_view return_name;
    struct arg* in_args;
    struct arg* cb_args;
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

                if (str_view_eq("prefix", option, p->data))
                    root->prefix = p->value.str;
                else if (str_view_eq("malloc", option, p->data))
                    root->malloc = p->value.str;
                else if (str_view_eq("free", option, p->data))
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
                enum token tok;
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
            expect_next_param: tok = scan_next_token(p);
            switch_next_param:
                switch (tok)
                {
                    case ')': break;
                    case ',':
                        if (query->in_args == NULL)
                            return print_error("Error: Expected parameter after \"(\"\n");
                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: Expected parameter after \",\"\n");
                        /* fallthrough */
                    case TOK_LABEL: {
                        struct arg* arg = arg_alloc();
                        arg->type = p->value.str;

                        if (query->in_args == NULL)
                            query->in_args = arg;
                        else
                        {
                            struct arg* args = query->in_args;
                            while (args->next)
                                args = args->next;
                            args->next = arg;
                        }

                        /* Special case, struct -> expect another label */
                        if (str_view_eq("struct", arg->type, p->data))
                        {
                            if (scan_next_token(p) != TOK_LABEL)
                                return print_error("Error: Missing struct name after \"struct\"\n");
                            arg->type.len = p->value.str.off + p->value.str.len - arg->type.off;
                        }
                        /* Special case, const -> expect another label */
                        if (str_view_eq("const", arg->type, p->data))
                        {
                            if (scan_next_token(p) != TOK_LABEL)
                                return print_error("Error: const qualifier without type\n");
                            arg->type.len = p->value.str.off + p->value.str.len - arg->type.off;
                        }

                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: Missing parameter name\n");
                        arg->name = p->value.str;

                        /* Param can have a "null" qualifier on the end */
                        if ((tok = scan_next_token(p)) == TOK_LABEL)
                        {
                            if (str_view_eq("null", p->value.str, p->data))
                                arg->nullable = 1;
                            else
                                return print_error("Error: Unknown parameter qualifier \"%.*s\"\n",
                                    p->value.str.len, p->data + p->value.str.off);
                            goto expect_next_param;
                        }
                    } goto switch_next_param;

                    default:
                        return print_error("Error: Expected parameter list\n");
                }

                if (scan_next_token(p) != '{')
                    return print_error("Error: Expected \"{\"\n");

            expect_next_stmt: tok = scan_next_token(p);
            switch_next_stmt:
                switch (tok) {
                    case TOK_TYPE: {
                        struct str_view t;
                        if (scan_next_token(p) != TOK_LABEL)
                            return print_error("Error: Expected query type after \"type\"\n");
                        t = p->value.str;
                        if (str_view_eq("insert", t, p->data))
                            query->type = QUERY_INSERT;
                        else if (str_view_eq("exists", t, p->data))
                            query->type = QUERY_EXISTS;
                        else if (str_view_eq("select-first", t, p->data))
                            query->type = QUERY_SELECT_FIRST;
                        else if (str_view_eq("select-all", t, p->data))
                            query->type = QUERY_SELECT_ALL;
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

                    case TOK_CALLBACK: {
                    expect_next_cb_param: tok = scan_next_token(p);
                    switch_next_cb_param:
                        switch (tok)
                        {
                            case ',':
                                if (query->cb_args == NULL)
                                    return print_error("Error: Expected parameter after \"select\"\n");
                                if (scan_next_token(p) != TOK_LABEL)
                                    return print_error("Error: Expected parameter after \",\"\n");
                                /* fallthrough */
                            case TOK_LABEL: {
                                struct arg* arg = arg_alloc();
                                arg->type = p->value.str;

                                if (query->cb_args == NULL)
                                    query->cb_args = arg;
                                else
                                {
                                    struct arg* args = query->cb_args;
                                    while (args->next)
                                        args = args->next;
                                    args->next = arg;
                                }

                                /* Special case, struct -> expect another label */
                                if (str_view_eq("struct", arg->type, p->data))
                                {
                                    if (scan_next_token(p) != TOK_LABEL)
                                        return print_error("Error: Missing struct name after \"struct\"\n");
                                    arg->type.len = p->value.str.off + p->value.str.len - arg->type.off;
                                }
                                /* Special case, const -> expect another label */
                                if (str_view_eq("const", arg->type, p->data))
                                {
                                    if (scan_next_token(p) != TOK_LABEL)
                                        return print_error("Error: const qualifier without type\n");
                                    arg->type.len = p->value.str.off + p->value.str.len - arg->type.off;
                                }

                                if (scan_next_token(p) != TOK_LABEL)
                                    return print_error("Error: Missing parameter name\n");
                                arg->name = p->value.str;

                                /* Param can have a "null" qualifier on the end */
                                if ((tok = scan_next_token(p)) == TOK_LABEL)
                                {
                                    if (str_view_eq("null", p->value.str, p->data))
                                        arg->nullable = 1;
                                    else
                                        return print_error("Error: Unknown parameter qualifier \"%.*s\"\n",
                                            p->value.str.len, p->data + p->value.str.off);
                                    goto expect_next_cb_param;
                                }
                            } goto switch_next_cb_param;

                            default: goto switch_next_stmt;
                        }
                    }

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
                        if (str_view_eq("struct", arg->type, p->data))
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

static void
fprintf_func_name(FILE* fp, const struct query_group* g, const struct query* q, const char* data)
{
    if (g)
        fprintf(fp, "%.*s_", g->name.len, data + g->name.off);
    fprintf(fp, "%.*s", q->name.len, data + q->name.off);
}

static void
fprintf_func_decl(FILE* fp, const struct root* root, const struct query_group* g, const struct query* q, const char* data)
{
    struct arg* a;

    fprintf(fp, "static int" NL);
    fprintf_func_name(fp, g, q, data);
    fprintf(fp, "(struct %.*s* ctx",  root->prefix.len, data + root->prefix.off);

    a = q->in_args;
    while (a)
    {
        fprintf(fp, ", %.*s %.*s",
            a->type.len, data + a->type.off,
            a->name.len, data + a->name.off);
        a = a->next;
    }

    if (q->cb_args)
        fprintf(fp, ", int (*on_row)(");
    a = q->cb_args;
    while (a)
    {
        if (a != q->cb_args) fprintf(fp, ", ");
        fprintf(fp, "%.*s %.*s",
            a->type.len, data + a->type.off,
            a->name.len, data + a->name.off);
        a = a->next;
    }
    if (q->cb_args)
        fprintf(fp, ", void* user_data), void* user_data");

    fprintf(fp, ")");
}

static void
fprintf_func_ptr_decl(FILE* fp, const struct root* root, const struct query_group* g, const struct query* q, const char* data)
{
    struct arg* a;

    fprintf(fp, "int (*");
    fprintf_func_name(fp, g, q, data);
    fprintf(fp, ")(struct %.*s* ctx",  root->prefix.len, data + root->prefix.off);

    a = q->in_args;
    while (a)
    {
        fprintf(fp, ", %.*s %.*s",
            a->type.len, data + a->type.off,
            a->name.len, data + a->name.off);
        a = a->next;
    }

    if (q->cb_args)
        fprintf(fp, ", int (*on_row)(");
    a = q->cb_args;
    while (a)
    {
        if (a != q->cb_args) fprintf(fp, ", ");
        fprintf(fp, "%.*s %.*s",
            a->type.len, data + a->type.off,
            a->name.len, data + a->name.off);
        a = a->next;
    }
    if (q->cb_args)
        fprintf(fp, ", void* user_data), void* user_data");

    fprintf(fp, ")");
}

static void
fprintf_sqlite_prepare_stmt(FILE* fp, const struct root* root, const struct query_group* g, const struct query* q, const char* data)
{
    struct arg* a;

    fprintf(fp, "    if (ctx->");
    fprintf_func_name(fp, g, q, data);
    fprintf(fp, " == NULL)" NL);
    fprintf(fp, "        if ((ret = sqlite3_prepare_v2(ctx->db," NL);

    if (q->stmt.len)
    {
        int p = 0;
        for (; p != q->stmt.len; ++p)
            if (!isspace(data[q->stmt.off + p]) && data[q->stmt.off + p] != '\r' && data[q->stmt.off + p] != '\n')
                break;

        fprintf(fp, "            \"");
        for (; p != q->stmt.len; ++p)
        {
            if (data[q->stmt.off + p] == '\n')
            {
                for (; p != q->stmt.len; ++p)
                    if (!isspace(data[q->stmt.off + p + 1]))
                        break;
                if (p + 1 < q->stmt.len)
                    fprintf(fp, " \"" NL "            \"");
            }
            else if (data[q->stmt.off + p] != '\r')
                fprintf(fp, "%c", data[q->stmt.off + p]);
        }
        fprintf(fp, "\"," NL);
    }
    else switch (q->type)
    {
        case QUERY_INSERT:
            if (q->return_name.len || q->cb_args)
                fprintf(fp, "            \"INSERT INTO %.*s (", q->table_name.len, data + q->table_name.off);
            else
                fprintf(fp, "            \"INSERT OR IGNORE INTO %.*s (", q->table_name.len, data + q->table_name.off);

            a = q->in_args;
            while (a) {
                if (a != q->in_args) fprintf(fp, ", ");
                fprintf(fp, "%.*s", a->name.len, data + a->name.off);
                a = a->next;
            }
            fprintf(fp, ") VALUES (");
            a = q->in_args;
            while (a) {
                if (a != q->in_args) fprintf(fp, ", ");
                fprintf(fp, "?");
                a = a->next;
            }
            fprintf(fp, ")");

            if (q->return_name.len || q->cb_args)
            {
                struct str_view reinsert = q->return_name.len ?
                    q->return_name : q->cb_args->name;
                fprintf(fp, " \"" NL);
                fprintf(fp, "            \"ON CONFLICT DO UPDATE SET %.*s=excluded.%.*s RETURNING ",
                    reinsert.len, data + reinsert.off,
                    reinsert.len, data + reinsert.off);
                if (q->return_name.len)
                    fprintf(fp, "%.*s", q->return_name.len, data + q->return_name.off);
                a = q->cb_args;
                while (a) {
                    if (a != q->cb_args || q->return_name.len) fprintf(fp, ", ");
                    fprintf(fp, "%.*s", a->name.len, data + a->name.off);
                    a = a->next;
                }
                fprintf(fp, ";\"," NL);
            }
            else
                fprintf(fp, ";\"," NL);

            break;

        case QUERY_EXISTS:
            fprintf(fp, "            \"SELECT 1 FROM %.*s", q->table_name.len, data + q->table_name.off);
            a = q->in_args;
            while (a) {
                if (a == q->in_args) fprintf(fp, " \"" NL "            \"WHERE "); else fprintf(fp, " AND ");
                fprintf(fp, "%.*s=?", a->name.len, data + a->name.off);
                a = a->next;
            }
            fprintf(fp, " LIMIT 1;\"," NL);
            break;

        case QUERY_SELECT_FIRST:
        case QUERY_SELECT_ALL:
            fprintf(fp, "            \"SELECT ");
            if (q->return_name.len)
                fprintf(fp, "%.*s", q->return_name.len, data + q->return_name.off);
            a = q->cb_args;
            while (a) {
                if (a != q->cb_args || q->return_name.len) fprintf(fp, ", ");
                fprintf(fp, "%.*s", a->name.len, data + a->name.off);
                a = a->next;
            }
            fprintf(fp, " FROM %.*s", q->table_name.len, data + q->table_name.off);

            a = q->in_args;
            while (a) {
                if (a == q->in_args) fprintf(fp, " \"" NL "            \"WHERE "); else fprintf(fp, " AND ");
                fprintf(fp, "%.*s=?", a->name.len, data + a->name.off);
                a = a->next;
            }

            if (q->type == QUERY_SELECT_FIRST)
                fprintf(fp, " LIMIT 1");

            fprintf(fp, ";\"," NL);
            break;
    }

    fprintf(fp, "            -1, &ctx->");
    fprintf_func_name(fp, g, q, data);
    fprintf(fp, ", NULL)) != SQLITE_OK)" NL);
    fprintf(fp, "        {" NL);
    fprintf(fp, "            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));" NL);
    fprintf(fp, "            return -1;" NL);
    fprintf(fp, "        }" NL);
    fprintf(fp, "\n");
}

static void
fprintf_sqlite_bind_args(FILE* fp, const struct root* root, const struct query_group* g, const struct query* q, const char* data)
{
    struct arg* a = q->in_args;
    int i = 1;
    for (; a; a = a->next, i++)
    {
        const char* sqlite_type = "";
        const char* cast = "";

        if (a == q->in_args) fprintf(fp, "    if ((ret = ");
        else                 fprintf(fp, "        (ret = ");

        if (str_view_eq("uint64_t", a->type, data))
            { sqlite_type = "int64"; cast = "(int64_t)"; }
        else if (str_view_eq("int64_t", a->type, data))
            { sqlite_type = "int64"; }
        else if (str_view_eq("int", a->type, data))
            { sqlite_type = "int"; }
        else if (str_view_eq("struct str_view", a->type, data))
            { sqlite_type = "text"; }
        else if (str_view_eq("const char*", a->type, data))
            { sqlite_type = "text"; }

        if (a->nullable)
        {
            fprintf(fp, "%.*s < 0 ? sqlite3_bind_null(ctx->", a->name.len, data + a->name.off);
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ", %d) : ", i);
        }
        fprintf(fp, "sqlite3_bind_%s(ctx->", sqlite_type);
        fprintf_func_name(fp, g, q, data);
        fprintf(fp, ", %d, %s%.*s", i, cast, a->name.len, data + a->name.off);

        if (str_view_eq("struct str_view", a->type, data))
            fprintf(fp, ".data, %.*s.len, SQLITE_STATIC", a->name.len, data + a->name.off);
        else if (str_view_eq("const char*", a->type, data))
            fprintf(fp, ", -1, SQLITE_STATIC");
        fprintf(fp, ")) != SQLITE_OK");

        if (a->next)
            fprintf(fp, " ||" NL);
    }
    fprintf(fp, ")" NL "    {" NL);
    fprintf(fp, "        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));" NL);
    fprintf(fp, "        return -1;" NL "    }" NL NL);
}

static void
fprintf_sqlite_exec_callback(FILE* fp, const struct query_group* g, const struct query* q, const char* data)
{
    struct arg* a = q->cb_args;
    int i = q->return_name.len ? 1 : 0;

    fprintf(fp, "            ret = on_row(" NL);
    for (; a; a = a->next, i++)
    {
        const char* sqlite_type = "";
        const char* cast = "";

        if (str_view_eq("uint64_t", a->type, data))
            { sqlite_type = "int64"; cast = "(uint64_t)"; }
        else if (str_view_eq("int64_t", a->type, data))
            sqlite_type = "int64";
        else if (str_view_eq("int", a->type, data))
            sqlite_type = "int";
        else if (str_view_eq("struct str_view", a->type, data))
            { sqlite_type = "text"; cast = "(const char*)"; }
        else if (str_view_eq("const char*", a->type, data))
            { sqlite_type = "text"; cast = "(const char*)"; }

        fprintf(fp, "                %ssqlite3_column_%s(ctx->", cast, sqlite_type);
        fprintf_func_name(fp, g, q, data);
        fprintf(fp, ", %d)," NL, i);
    }
    fprintf(fp, "                user_data);" NL);
}

static void
fprintf_sqlite_exec(FILE* fp, const struct root* root, const struct query_group* g, const struct query* q, const char* data)
{
    struct arg* a;
    int i;
    switch (q->type)
    {
        case QUERY_EXISTS:
            fprintf(fp, "next_step:" NL);
            fprintf(fp, "    ret = sqlite3_step(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ");" NL);
            fprintf(fp, "    switch (ret)" NL "    {" NL);
            fprintf(fp, "        case SQLITE_ROW:" NL);
            fprintf(fp, "            sqlite3_reset(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, "); " NL);
            fprintf(fp, "            return 1;" NL);
            fprintf(fp, "        case SQLITE_BUSY: goto next_step;" NL);
            fprintf(fp, "        case SQLITE_DONE:" NL);
            fprintf(fp, "            sqlite3_reset(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ");" NL);
            fprintf(fp, "            return 0;" NL);
            fprintf(fp, "    }" NL NL);
            fprintf(fp, "    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));" NL);
            fprintf(fp, "    sqlite3_reset(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ");" NL);
            fprintf(fp, "    return -1;" NL);
            break;

        case QUERY_INSERT:
            fprintf(fp, "next_step:" NL);
            fprintf(fp, "    ret = sqlite3_step(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ");" NL);
            fprintf(fp, "    switch (ret)" NL "    {" NL);

            if (q->return_name.len || q->cb_args)
                fprintf(fp, "        case SQLITE_ROW:" NL);

            if (q->return_name.len)
            {
                fprintf(fp, "            %.*s = sqlite3_column_int(ctx->",
                    q->return_name.len, data + q->return_name.off);
                fprintf_func_name(fp, g, q, data);
                fprintf(fp, ", 0);" NL);
            }

            if (q->cb_args)
                fprintf_sqlite_exec_callback(fp, g, q, data);

            if (q->return_name.len || q->cb_args)
            {
                fprintf(fp, "            sqlite3_reset(ctx->");
                fprintf_func_name(fp, g, q, data);
                fprintf(fp, ");" NL);

                if (q->return_name.len)
                    fprintf(fp, "            return %.*s;" NL, q->return_name.len, data + q->return_name.off);
                else
                    fprintf(fp, "            return ret;" NL);
            }

            fprintf(fp, "        case SQLITE_BUSY: goto next_step;" NL);
            fprintf(fp, "        case SQLITE_DONE:" NL);
            fprintf(fp, "            sqlite3_reset(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ");" NL);
            fprintf(fp, "            return 0;" NL);
            fprintf(fp, "    }" NL NL);
            fprintf(fp, "    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));" NL);
            fprintf(fp, "    sqlite3_reset(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ");" NL);
            fprintf(fp, "    return -1;" NL);
            break;

        case QUERY_SELECT_FIRST:
        case QUERY_SELECT_ALL:
            fprintf(fp, "next_step:" NL);
            fprintf(fp, "    ret = sqlite3_step(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ");" NL);
            fprintf(fp, "    switch (ret)" NL "    {" NL);
            fprintf(fp, "        case SQLITE_ROW:" NL);

            if (q->return_name.len)
            {
                fprintf(fp, "            %.*s = sqlite3_column_int(ctx->",
                    q->return_name.len, data + q->return_name.off);
                fprintf_func_name(fp, g, q, data);
                fprintf(fp, ", 0);" NL);
            }

            if (q->cb_args)
                fprintf_sqlite_exec_callback(fp, g, q, data);

            if (q->type != QUERY_SELECT_FIRST && q->cb_args)
            {
                fprintf(fp, "            if (ret == 0) goto next_step;" NL);

                fprintf(fp, "            sqlite3_reset(ctx->");
                fprintf_func_name(fp, g, q, data);
                fprintf(fp, ");" NL);

                if (q->return_name.len)
                    fprintf(fp, "            return %.*s;" NL, q->return_name.len, data + q->return_name.off);
                else
                    fprintf(fp, "            return ret;" NL);
            }

            fprintf(fp, "        case SQLITE_BUSY: goto next_step;" NL);
            fprintf(fp, "        case SQLITE_DONE:" NL);
            fprintf(fp, "            sqlite3_reset(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ");" NL);

            if (q->return_name.len)
                fprintf(fp, "            return %.*s;" NL, q->return_name.len, data + q->return_name.off);
            else
                fprintf(fp, "            return 0;" NL);

            fprintf(fp, "    }" NL NL);
            fprintf(fp, "    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));" NL);
            fprintf(fp, "    sqlite3_reset(ctx->");
            fprintf_func_name(fp, g, q, data);
            fprintf(fp, ");" NL);
            fprintf(fp, "    return -1;" NL);
            break;
    }
}

static int
gen_header(const struct root* root, const char* data, const char* file_name)
{
    struct query* q;
    struct query_group* g;
    struct function* f;
    struct arg* a;
    FILE* fp = fopen(file_name, "wb");
    if (fp == NULL)
        return print_error("Error: Failed to open file for writing \"%s\"\n", file_name);

    if (root->header_preamble.len)
        fprintf(fp, "%.*s" NL, root->header_preamble.len, data + root->header_preamble.off);

    fprintf(fp, "struct %.*s;" NL, root->prefix.len, data + root->prefix.off);
    fprintf(fp, "struct %.*s_interface" NL "{" NL, root->prefix.len, data + root->prefix.off);

    /* Open and close functions are hard-coded */
    fprintf(fp, "    struct %.*s* (*open)(const char* uri);" NL,
            root->prefix.len, data + root->prefix.off);
    fprintf(fp, "    void (*close)(struct %.*s* ctx);" NL NL,
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
        fprintf(fp, ");" NL);

        f = f->next;
    }

    /* Global queries */
    q = root->queries;
    while (q)
    {
        fprintf(fp, "    ");
        fprintf_func_ptr_decl(fp, root, NULL, q, data);
        fprintf(fp, ";" NL);
        q = q->next;
    }
    fprintf(fp, NL);

    /* Grouped queries */
    g = root->query_groups;
    while (g)
    {
        fprintf(fp, "    struct {" NL);
        q = g->queries;
        while (q)
        {
            fprintf(fp, "        ");
            fprintf_func_ptr_decl(fp, root, NULL, q, data);
            fprintf(fp, ";" NL);
            q = q->next;
        }
        fprintf(fp, "    } %.*s;" NL NL, g->name.len, data + g->name.off);
        g = g->next;
    }
    fprintf(fp, "};" NL NL);

    if (root->header_postamble.len)
        fprintf(fp, "%.*s" NL, root->header_postamble.len, data + root->header_postamble.off);

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
    FILE* fp = fopen(file_name, "wb");
    if (fp == NULL)
        return print_error("Error: Failed to open file for writing \"%s\"" NL, file_name);

    if (root->source_includes.len)
        fprintf(fp, "%.*s" NL, root->source_includes.len, data + root->source_includes.off);

    /* ------------------------------------------------------------------------
     * Context structure declaration
     * --------------------------------------------------------------------- */

    fprintf(fp, "struct %.*s" NL "{" NL,
            root->prefix.len, data + root->prefix.off);
    fprintf(fp, "    sqlite3* db;" NL);
    /* Global queries */
    q = root->queries;
    while (q)
    {
        fprintf(fp, "    sqlite3_stmt* %.*s;" NL, q->name.len, data + q->name.off);
        q = q->next;
    }
    /* Grouped queries */
    g = root->query_groups;
    while (g)
    {
        q = g->queries;
        while (q)
        {
            fprintf(fp, "    sqlite3_stmt* %.*s_%.*s;" NL,
                    g->name.len, data + g->name.off,
                    q->name.len, data + q->name.off);
            q = q->next;
        }
        g = g->next;
    }
    fprintf(fp, "};" NL);

    if (root->source_preamble.len)
        fprintf(fp, "%.*s" NL, root->source_preamble.len, data + root->source_preamble.off);

    /* ------------------------------------------------------------------------
     * Functions
     * --------------------------------------------------------------------- */

    f = root->functions;
    while (f)
    {
        fprintf(fp, "static int" NL "%.*s(struct %.*s* ctx",
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
        fprintf(fp, ")" NL "{");
        fprintf(fp, "%.*s", f->body.len, data + f->body.off);
        fprintf(fp, "}" NL NL);

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
            fprintf_func_decl(fp, root, g, q, data);
            fprintf(fp, NL "{" NL);

            /* Local variables */
            fprintf(fp, "    int ret");
            if (q->return_name.len)
                fprintf(fp, ", %.*s = -1", q->return_name.len, data + q->return_name.off);
            fprintf(fp, ";" NL);

            fprintf_sqlite_prepare_stmt(fp, root, g, q, data);
            fprintf_sqlite_bind_args(fp, root, g, q, data);
            fprintf_sqlite_exec(fp, root, g, q, data);

            fprintf(fp, "}" NL NL);

            q = q->next;
        }
        g = g->next;
    }

    /* ------------------------------------------------------------------------
     * Open and close
     * --------------------------------------------------------------------- */

    fprintf(fp, "static struct %.*s*" NL "%.*s_open(const char* uri)" NL "{" NL,
            root->prefix.len, data + root->prefix.off,
            root->prefix.len, data + root->prefix.off);
    fprintf(fp, "    int ret;" NL);
    fprintf(fp, "    struct %.*s* ctx = %.*s(sizeof *ctx);" NL,
            root->prefix.len, data + root->prefix.off,
            root->malloc.len ? root->malloc.len : 6, root->malloc.len ? data + root->malloc.off : "malloc");
    fprintf(fp, "    if (ctx == NULL)" NL);
    fprintf(fp, "        return NULL;" NL);
    fprintf(fp, "    memset(ctx, 0, sizeof *ctx);" NL NL);
    fprintf(fp, "    ret = sqlite3_open_v2(uri, &ctx->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);" NL);
    fprintf(fp, "    if (ret == SQLITE_OK)" NL);
    fprintf(fp, "        return ctx;" NL NL);
    fprintf(fp, "    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));" NL);
    fprintf(fp, "    %.*s(ctx);" NL,
            root->free.len ? root->free.len : 6, root->free.len ? data + root->free.off : "free");
    fprintf(fp, "    return NULL;" NL);
    fprintf(fp, "}" NL NL);

    fprintf(fp, "static void" NL "%.*s_close(struct %.*s* ctx)" NL "{" NL,
            root->prefix.len, data + root->prefix.off,
            root->prefix.len, data + root->prefix.off);
    /* Global queries */
    q = root->queries;
    while (q)
    {
        fprintf(fp, "    sqlite3_finalize(ctx->%.*s);" NL, q->name.len, data + q->name.off);
        q = q->next;
    }
    /* Grouped queries */
    g = root->query_groups;
    while (g)
    {
        q = g->queries;
        while (q)
        {
            fprintf(fp, "    sqlite3_finalize(ctx->%.*s_%.*s);" NL,
                    g->name.len, data + g->name.off,
                    q->name.len, data + q->name.off);
            q = q->next;
        }
        g = g->next;
    }
    fprintf(fp, "    sqlite3_close(ctx->db);" NL);
    fprintf(fp, "    %.*s(ctx);" NL,
            root->free.len ? root->free.len : 6, root->free.len ? data + root->free.off : "free");
    fprintf(fp, "}" NL NL);

    /* ------------------------------------------------------------------------
     * Interface
     * --------------------------------------------------------------------- */

    fprintf(fp, "static struct %.*s_interface db_sqlite = {" NL, root->prefix.len, data + root->prefix.off);
    fprintf(fp, "    %.*s_open," NL "    %.*s_close," NL,
            root->prefix.len, data + root->prefix.off,
            root->prefix.len, data + root->prefix.off);
    /* Functions */
    f = root->functions;
    while (f)
    {
        fprintf(fp, "    %.*s," NL, f->name.len, data + f->name.off);
        f = f->next;
    }
    /* Global queries */
    q = root->queries;
    while (q)
    {
        fprintf(fp, "    %.*s," NL, q->name.len, data + q->name.off);
        q = q->next;
    }
    /* Grouped queries */
    g = root->query_groups;
    while (g)
    {
        fprintf(fp, "    {" NL);
        q = g->queries;
        while (q)
        {
            fprintf(fp, "        %.*s_%.*s," NL,
                    g->name.len, data + g->name.off,
                    q->name.len, data + q->name.off);
            q = q->next;
        }
        fprintf(fp, "    }," NL);
        g = g->next;
    }
    fprintf(fp, "};" NL NL);

    if (root->source_postamble.len)
        fprintf(fp, "%.*s" NL, root->source_postamble.len, data + root->source_postamble.off);

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
    if (parse(&parser, &root) != 0)
        return -1;

    post_parse(&root, mf.address);

    if (gen_header(&root, mf.address, cfg.output_header) < 0)
        return -1;
    if (gen_source(&root, mf.address, cfg.output_source) < 0)
        return -1;

    return 0;
}
