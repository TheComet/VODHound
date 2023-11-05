#pragma once

struct ast;
struct db;
struct db_interface;

/*
 * Tries to replace all AST_LABEL nodes with AST_MOTION nodes. Labels are first
 * matched with user-defined labels by querying the db's motion_labels table.
 * This can cause the AST to create more nodes, since user-defined labels can
 * match multiple different motions.
 *
 * If this fails, then the function falls back on converting the label directly
 * to a motion value using hash40_str(). If the calculated value is not found
 * in the database, in the "motions" table, then an error is returned. This is
 * to ensure that the user can't just type in any string, but technically, the
 * AST would still compile in this situation.
 *
 * Nodes that were not found can be converted separately without checking the
 * db with the function ast_post_hash40_remaining_labels().
 */
int
ast_post_labels_to_motions(struct ast* ast, struct db_interface* dbi, struct db* db, int fighter_id);
void
ast_post_hash40_remaining_labels(struct ast* ast);

/* These are called internally after parsing in parser_parse() */
int ast_post_dj(struct ast* ast);
int ast_post_fh(struct ast* ast);
int ast_post_fs(struct ast* ast);
int ast_post_idj(struct ast* ast);
int ast_post_sh(struct ast* ast);
int ast_post_timing(struct ast* ast);
int ast_post_validate_params(struct ast* ast);
