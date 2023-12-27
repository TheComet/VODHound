#pragma once

#include "vh/config.h"

C_BEGIN

struct mfile
{
    void* address;
    int size;
};

/*!
 * \brief Memory-maps a file in read-only mode. If the memory is written
 * to, then the data is copied (COW) and no changes are written back to the
 * original file.
 * \param[in] mf Pointer to mfile structure. Struct can be uninitialized.
 * \param[in] file_path Utf8 encoded file path.
 * \return Returns 0 on success, negative on failure.
 */
VH_PUBLIC_API int
mfile_map_read(struct mfile* mf, const char* file_name);

/*! \brief Unmap a previously mapped file. */
VH_PUBLIC_API void
mfile_unmap(struct mfile* mf);

C_END
