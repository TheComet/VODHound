#pragma once

struct ast;
struct db;
struct db_interface;

int
ast_post_patch_motions(struct ast* ast, struct db_interface* dbi, struct db* db, int fighter_id);

int
ast_post_dj(struct ast* ast);
int
ast_post_fh(struct ast* ast);
int
ast_post_fs(struct ast* ast);
int
ast_post_idj(struct ast* ast);
int
ast_post_sh(struct ast* ast);
