#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum backend
{
    BACKEND_SQLITE3 = 0x01
};

struct cfg
{
    const char* input_file_name;
    const char* input_file_path;
    const char* output_dir;
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

            cfg->input_file_path = argv[++i];

            cfg->input_file_name = cfg->input_file_path + strlen(cfg->input_file_path);
            while (*cfg->input_file_name != '/' && *cfg->input_file_name != '\\' && cfg->input_file_name > cfg->input_file_path)
                cfg->input_file_name--;
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: Missing argument to option -o\n");
                return -1;
            }

            cfg->output_dir = argv[++i];
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
    }

    if (cfg->backends == 0)
    {
        fprintf(stderr, "Error: No backends were specified. Use -b <backend1,backend2,...>. Supported backends are: sqlite\n");
        return -1;
    }

    return 0;
}

int main(int argc, char** argv)
{
    struct cfg cfg = { 0 };
    if (parse_cmdline(argc, argv, &cfg) != 0)
        return -1;
    
    return 0;
}
