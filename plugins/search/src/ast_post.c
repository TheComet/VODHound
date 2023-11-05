#include "search/ast.h"
#include "search/ast_ops.h"
#include "search/ast_post.h"

#include "vh/db_ops.h"
#include "vh/hash40.h"
#include "vh/hm.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/str.h"
#include "vh/vec.h"

static int
try_patch_user_defined(
    struct ast* ast, int node,
    struct db_interface* dbi, struct db* db, int fighter_id,
    struct vec* motions,
    struct str_view label)
{

    /*
     * If the label is a user-defined label, for example "nair", then it
     * may map to more than 1 motion value, for example, "attack_air_n"
     * and "landing_air_n". The label node in the AST is replaced with
     * "(attack_air_n|landing_air_n)+". This will match all patterns of
     * "nair".
     */
    vec_clear(motions);
    switch (dbi->motion_label.to_motions(db, fighter_id, label, motions))
    {
        case 1: {
            int root = -1;
            VEC_FOR_EACH(motions, uint64_t, motion)
                struct strlist_str* hm_label;
                int node = ast_motion(ast, *motion, (struct YYLTYPE*)&ast->nodes[node].info.loc);
                if (node < 0)
                    return -1;

                root = root == -1 ? node :
                    ast_union(ast, root, node, (struct YYLTYPE*)&ast->nodes[node].info.loc);
                if (root < 0)
                    return -1;

                switch (hm_insert(&ast->merged_labels, motion, (void**)&hm_label))
                {
                    case 1  : *hm_label = ast->nodes[node].label.label;
                    case 0  : break;
                    default : return -1;
                }
            VEC_END_EACH

            if (root >= 0)
            {
                root = ast_repetition(ast, root, 1, -1, (struct YYLTYPE*)&ast->nodes[node].info.loc);
                if (root < 0)
                    return -1;
                ast_collapse_into(ast, root, node);
            }

            return 1;
        }

        case 0  : return 0;
        default : return -1;
    }
}

static int
try_patch_existing_hash40(
    struct ast* ast, int node,
    struct db_interface* dbi, struct db* db,
    struct str_view label)
{
    /* Maybe the label hashes to a known value. */
    uint64_t motion = hash40_str(label);
    switch (dbi->motion.exists(db, motion))
    {
        case 1  : break;
        case 0  : return 0;
        default : return -1;
    }

    /* It exists, so replace the label node with a motion node */
    ast->nodes[node].info.type = AST_MOTION;
    ast->nodes[node].motion.motion = motion;
    return 1;
}

int
ast_post_labels_to_motions(struct ast* ast, struct db_interface* dbi, struct db* db, int fighter_id)
{
    struct vec motions;
    int n, root;

    vec_init(&motions, sizeof(uint64_t));

    for (n = 0; n != ast->node_count; ++n)
    {
        struct str_view label;
        if (ast->nodes[n].info.type != AST_LABEL)
            continue;
        label = strlist_to_view(&ast->labels, ast->nodes[n].label.label);

        switch (try_patch_user_defined(ast, n, dbi, db, fighter_id, &motions, label))
        {
            case 1  : continue;
            case 0  : break;
            default : goto error;
        }

        switch (try_patch_existing_hash40(ast, n, dbi, db, label))
        {
            case 1  : continue;
            case 0  : break;
            default : goto error;
        }

        /* Was unable to find a label that matches a motion value. Error out */
        log_err("No motion value found for label '%.*s'\n", label.len, label.data);
        goto error;
    }

    vec_deinit(&motions);
    return 0;

error:
    vec_deinit(&motions);
    return -1;
}

void
ast_post_hash40_remaining_labels(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
    {
        struct str_view label;
        if (ast->nodes[n].info.type != AST_LABEL)
            continue;
        label = strlist_to_view(&ast->labels, ast->nodes[n].label.label);

        ast->nodes[n].info.type = AST_MOTION;
        ast->nodes[n].motion.motion = hash40_str(label);
    }
}

int
ast_post_dj(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        if (ast->nodes[n].info.type == AST_CONTEXT_QUALIFIER &&
            (ast->nodes[n].context_qualifier.flags & AST_CTX_DJ))
        {
            int jump_aerial_f, jump_aerial_b, union_, dj, dj_n;
            const struct YYLTYPE* loc = (const struct YYLTYPE*)&ast->nodes[n].info.loc;

            /* hash40("jump_aerial_f") = 0xd0b71815b */
            jump_aerial_f = ast_motion(ast, 0xd0b71815bul, loc);
            if (jump_aerial_f < 0) return -1;

            /* hash40("jump_aerial_b") = 0xd0c1c4542 */
            jump_aerial_b = ast_motion(ast, 0xd0c1c4542ul, loc);
            if (jump_aerial_b < 0) return -1;

            /* jump_aerial_f | jump_aerial_b */
            union_ = ast_union(ast, jump_aerial_f, jump_aerial_b, loc);
            if (union_ < 0) return -1;

            /* (jump_aerial_f | jump_aerial_b)+ */
            dj = ast_repetition(ast, union_, 1, -1, loc);
            if (dj < 0) return -1;

            dj_n = ast_statement(ast, dj, ast->nodes[n].context_qualifier.child, loc);
            if (dj_n < 0) return -1;

            ast_collapse_into(ast, dj_n, n);
        }

    return 0;
}

int
ast_post_fh(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        if (ast->nodes[n].info.type == AST_CONTEXT_QUALIFIER &&
            (ast->nodes[n].context_qualifier.flags & AST_CTX_FH))
        {
            int jump_f, jump_b, union_, dj, dj_n;
            const struct YYLTYPE* loc = (const struct YYLTYPE*)&ast->nodes[n].info.loc;

            /* hash40("jump_f") = 0x62dd02058 */
            jump_f = ast_motion(ast, 0x62dd02058ul, loc);
            if (jump_f < 0) return -1;

            /* hash40("jump_b") = 0x62abde441 */
            jump_b = ast_motion(ast, 0x62abde441ul, loc);
            if (jump_b < 0) return -1;

            /* jump_f | jump_b */
            union_ = ast_union(ast, jump_f, jump_b, loc);
            if (union_ < 0) return -1;

            /* (jump_f | jump_b)+ */
            dj = ast_repetition(ast, union_, 1, -1, loc);
            if (dj < 0) return -1;

            dj_n = ast_statement(ast, dj, ast->nodes[n].context_qualifier.child, loc);
            if (dj_n < 0) return -1;

            ast_collapse_into(ast, dj_n, n);
        }

    return 0;
}

int
ast_post_fs(struct ast* ast)
{
    return 0;
}

int
ast_post_idj(struct ast* ast)
{
    /*
     * idj decomposes to jump_squat -> sh|fh -> f1 dj
     * where:
     *   sh = jump_b_mini | jump_f_mini
     *   fh = jump_b | jump_f
     *   dj = jump_aerial_b | jump_aerial_f
     */
    int n;
    for (n = 0; n != ast->node_count; ++n)
        if (ast->nodes[n].info.type == AST_CONTEXT_QUALIFIER &&
            (ast->nodes[n].context_qualifier.flags & AST_CTX_IDJ))
        {
            int jump_squat;
            int jump_f, jump_b, jump_f_mini, jump_b_mini, sh, fh;
            int jump_aerial_f, jump_aerial_b;
            int jump, dj;
            int f1;
            int into1, into2, into3;
            const struct YYLTYPE* loc = (const struct YYLTYPE*)&ast->nodes[n].info.loc;

            /* hash40("jump_squat") = 0xad160bda8 */
            jump_squat = ast_motion(ast, 0xad160bda8, loc);
            if (jump_squat < 0) return -1;

            /* hash40("jump_f") = 0x62dd02058 */
            jump_f = ast_motion(ast, 0x62dd02058ul, loc);
            if (jump_f < 0) return -1;
            /* hash40("jump_b") = 0x62abde441 */
            jump_b = ast_motion(ast, 0x62abde441ul, loc);
            if (jump_b < 0) return -1;
            /* hash40("jump_f_mini") = 0xb38c9ab48 */
            jump_f_mini = ast_motion(ast, 0xb38c9ab48ul, loc);
            if (jump_f_mini < 0) return -1;
            /* hash40("jump_b_mini") = 0xba358e95e */
            jump_b_mini = ast_motion(ast, 0xba358e95eul, loc);
            if (jump_b_mini < 0) return -1;
            sh = ast_union(ast, jump_f_mini, jump_b_mini, loc);
            if (sh < 0) return -1;
            fh = ast_union(ast, jump_f, jump_b, loc);
            if (fh < 0) return -1;
            jump = ast_union(ast, sh, fh, loc);
            if (jump < 0) return -1;

            /* hash40("jump_aerial_f") = 0xd0b71815b */
            jump_aerial_f = ast_motion(ast, 0xd0b71815bul, loc);
            if (jump_aerial_f < 0) return -1;
            /* hash40("jump_aerial_b") = 0xd0c1c4542 */
            jump_aerial_b = ast_motion(ast, 0xd0c1c4542ul, loc);
            if (jump_aerial_b < 0) return -1;
            dj = ast_union(ast, jump_aerial_f, jump_aerial_b, loc);
            if (dj < 0) return -1;

            /* With IDJs, the double jump has to occur frame 1 */
            f1 = ast_timing(ast, dj, -1, 1, -1, loc);
            if (f1 < 0) return -1;

            into1 = ast_statement(ast, jump_squat, jump, loc);
            if (into1 < 0) return -1;
            into2 = ast_statement(ast, into1, f1, loc);
            if (into2 < 0) return -1;

            into3 = ast_statement(ast, into2, ast->nodes[n].context_qualifier.child, loc);
            ast_collapse_into(ast, into3, n);
        }

    return 0;
}

int
ast_post_sh(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        if (ast->nodes[n].info.type == AST_CONTEXT_QUALIFIER &&
            (ast->nodes[n].context_qualifier.flags & AST_CTX_SH))
        {
            int jump_f_mini, jump_b_mini, union_, dj, dj_n;
            const struct YYLTYPE* loc = (const struct YYLTYPE*)&ast->nodes[n].info.loc;

            /* hash40("jump_f_mini") = 0xb38c9ab48 */
            jump_f_mini = ast_motion(ast, 0xb38c9ab48ul, loc);
            if (jump_f_mini < 0) return -1;

            /* hash40("jump_b_mini") = 0xba358e95e */
            jump_b_mini = ast_motion(ast, 0xba358e95eul, loc);
            if (jump_b_mini < 0) return -1;

            /* jump_f_mini | jump_b_mini */
            union_ = ast_union(ast, jump_f_mini, jump_b_mini, loc);
            if (union_ < 0) return -1;

            /* (jump_f_mini | jump_b_mini)+ */
            dj = ast_repetition(ast, union_, 1, -1, loc);
            if (dj < 0) return -1;

            dj_n = ast_statement(ast, dj, ast->nodes[n].context_qualifier.child, loc);
            if (dj_n < 0) return -1;

            ast_collapse_into(ast, dj_n, n);
        }

    return 0;
}

int
ast_post_timing(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        if (ast->nodes[n].info.type == AST_TIMING)
        {
            if (ast->nodes[n].timing.end == -1)
                ast->nodes[n].timing.end = ast->nodes[n].timing.start;

            if (ast->nodes[n].timing.rel_to < 0)
            {
                int prev_stmt = n;
                /* Travel up tree until we find the statement that owns the
                 * current node */
                while (1)
                {
                    int n2;
                    for (n2 = 0; n2 != ast->node_count; ++n2)
                    {
                        if (ast->nodes[n2].base.right == prev_stmt)
                        {
                            prev_stmt = n2;

                            /* If we came from the right side of a statement node, in all cases,
                             * the "previous statement" will be on the left branch */
                            if (ast->nodes[prev_stmt].info.type == AST_STATEMENT)
                                goto found_top_most;
                            break;
                        }
                        else if (ast->nodes[n2].base.left == prev_stmt)
                        {
                            /* Reached the top-most statement */
                            if (ast->nodes[prev_stmt].info.type == AST_STATEMENT &&
                                ast->nodes[n2].info.type != AST_STATEMENT)
                            {
                                goto found_top_most;
                            }
                            prev_stmt = n2;
                            break;
                        }
                    }

                    if (prev_stmt == 0)
                        break;
                } found_top_most:;

                if (ast->nodes[prev_stmt].info.type != AST_STATEMENT)
                {
                    log_err("Timing isn't relative to anything! Need to have a previous statement\n");
                    return -1;
                }

                /* Travel down right side of the statement's chain to find the final
                 * "previous statement" */
                prev_stmt = ast->nodes[prev_stmt].base.left;
                while (ast->nodes[prev_stmt].info.type == AST_STATEMENT)
                    prev_stmt = ast->nodes[prev_stmt].base.right;

                ast->nodes[n].timing.rel_to = prev_stmt;
            }
            else
            {
                /* TODO */
                return -1;
            }
        }
    return 0;
}

int
ast_post_validate_params(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        switch (ast->nodes[n].info.type)
        {
            case AST_REPETITION : {
                int min_reps = ast->nodes[n].repetition.min_reps;
                int max_reps = ast->nodes[n].repetition.max_reps;

                if (min_reps < 0)
                {
                    log_err("Cannot repeat from '%d' times\n", min_reps);
                    return -1;
                }

                /* No upper bound */
                if (max_reps == -1)
                    continue;

                if (max_reps < 0 || min_reps > max_reps)
                {
                    log_err("Cannot repeat from '%d' to '%d' times (min>max)\n", min_reps, max_reps);
                    return -1;
                }
            } break;

            case AST_CONTEXT_QUALIFIER: {
            } break;

            case AST_TIMING: {
                int start = ast->nodes[n].timing.start;
                int end = ast->nodes[n].timing.end;

                if (start < 1)
                {
                    log_err("Invalid starting frame '%d': Must be 1 or greater\n", start);
                    return -1;
                }

                if (start > end)
                {
                    log_err("Invalid frame range '%d' - '%d': Start frame must be smaller than end frame\n", start, end);
                    return -1;
                }
            } break;

            default: break;
        }

    return 0;
}
