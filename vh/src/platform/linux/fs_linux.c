#include "vh/fs.h"

#include <sys/types.h>
#include <dirent.h>

static int match_all(struct str_view str) { return 1; }

int
fs_dir_list(struct strlist* out, const char* path)
{
    return fs_dir_list_matching(out, path, match_all);
}

int
fs_dir_list_matching(struct strlist* out, const char* path, int (*match)(struct str_view str))
{
    DIR* dp;
    struct dirent* ep;
    
    dp = opendir(PLUGIN_DIR);
    if (!dp)
        return -1;
    
    while ((ep = readdir(dp)) != NULL)
    {
    }
    
    closedir(dp);
    return 0;
}
