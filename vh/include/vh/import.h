#pragma once

#include "vh/str.h"

struct db;
struct db_interface;

int
import_reframed_all(struct db_interface* dbi, struct db* db);

int
import_reframed_mapping_info(struct db_interface* dbi, struct db* db, const char* file_name);

int
import_param_labels_csv(struct db_interface* dbi, struct db* db, const char* csv_file_path);
