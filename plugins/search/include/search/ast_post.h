#pragma once

struct ast;
struct db;
struct db_interface;

int ast_post_patch_motions(struct ast* ast, struct db_interface* dbi, struct db* db, int fighter_id);
