#pragma once

#include "vh/config.h"

C_BEGIN

struct db;
struct db_interface;

VH_PUBLIC_API int
import_reframed_all(struct db_interface* dbi, struct db* db);

VH_PUBLIC_API int
import_reframed_mapping_info(struct db_interface* dbi, struct db* db, const char* file_name);

VH_PUBLIC_API int
import_param_labels_csv(struct db_interface* dbi, struct db* db, const char* csv_file_path);

C_END
