#define _LARGEFILE64_SOURCE
#include "rf/mfile.h"

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

int map_file(struct mfile* mf, const char* file_name)
{
    struct stat stbuf;
    int fd;

    fd = open(file_name, O_RDONLY | O_LARGEFILE);
    if (fd < 0)
        goto open_failed;

    if (fstat(fd, &stbuf) != 0)
        goto fstat_failed;
    if (!S_ISREG(stbuf.st_mode))
        goto fstat_failed;

    /*if (stbuf.st_size >= (1UL<<32))
        goto file_too_large;*/

    mf->address = mmap(NULL, (size_t)stbuf.st_size, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
    if (mf->address == NULL)
        goto mmap_failed;

    /* file descriptor no longer required */
    close(fd);

    mf->size = (int)stbuf.st_size;
    return 0;

    mmap_failed    :
    file_too_large :
    fstat_failed   : close(fd);
    open_failed    : return -1;
}

void unmap_file(struct mfile* mf)
{
    munmap(mf->address, (size_t)mf->size);
}
