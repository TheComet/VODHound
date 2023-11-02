#pragma once

union ast_node;
struct db;
struct db_interface;

int ast_post_patch_motions(union ast_node* root, struct db_interface* dbi, struct db* db, int fighter_id);
