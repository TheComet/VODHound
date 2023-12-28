#include "search/ast.h"
#include "search/ast_ops.h"
#include "search/ast_post.h"

#include "vh/db.h"
#include "vh/hash40.h"
#include "vh/hm.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/str.h"
#include "vh/vec.h"

struct on_motion_ctx
{
    struct ast* ast;
    int node;
    int replace_node;
    int row_count;
};

static int on_motion(uint64_t hash40, void* user_data)
{
    struct on_motion_ctx* ctx = user_data;
    struct strlist_str* hm_label;
    struct YYLTYPE* loc = (struct YYLTYPE*)&ctx->ast->nodes[ctx->node].info.loc;
    int n = ast_motion(ctx->ast, hash40, loc);
    if (n < 0)
        return -1;

    ctx->replace_node = ctx->replace_node == -1 ? n :
        ast_union(ctx->ast, ctx->replace_node, n, loc);
    if (ctx->replace_node < 0)
        return -1;

    switch (hm_insert(&ctx->ast->merged_labels, &hash40, (void**)&hm_label))
    {
        case 1  : *hm_label = ctx->ast->nodes[ctx->node].label.label;
        case 0  : break;
        default : return -1;
    }

    ctx->row_count++;
    return 0;
}

static int
try_patch_user_defined(
    struct ast* ast, int node,
    struct db_interface* dbi, struct db* db, int fighter_id,
    struct str_view label)
{
    /*
     * If the label is a user-defined label, for example "nair", then it
     * may map to more than 1 motion value, for example, "attack_air_n"
     * and "landing_air_n". The label node in the AST is replaced with
     * "(attack_air_n|landing_air_n)+". This will match all patterns of
     * "nair".
     */
    struct on_motion_ctx ctx = { ast, node, -1, 0 };
    if (dbi->motion_label.to_motions(db, fighter_id, label, on_motion, &ctx) < 0)
        return -1;

    if (ctx.row_count == 0)
        return 0;

    /* Only need repetition if there is more than 1 label */
    if (ctx.row_count > 1)
    {
        ctx.replace_node = ast_repetition(ast, ctx.replace_node, 1, -1, (struct YYLTYPE*)&ast->nodes[node].info.loc);
        if (ctx.replace_node < 0)
            return -1;
    }

    ast_collapse_into(ast, ctx.replace_node, node);
    return 1;
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
    int n;

    for (n = 0; n != ast->node_count; ++n)
    {
        struct str_view label;
        if (ast->nodes[n].info.type != AST_LABEL)
            continue;
        label = strlist_to_view(&ast->labels, ast->nodes[n].label.label);

        switch (try_patch_user_defined(ast, n, dbi, db, fighter_id, label))
        {
            case 1  : continue;
            case 0  : break;
            default : return -1;
        }

        switch (try_patch_existing_hash40(ast, n, dbi, db, label))
        {
            case 1  : continue;
            case 0  : break;
            default : return -1;
        }

        /* Was unable to find a label that matches a motion value. Error out */
        log_err("No motion value found for label '%.*s'\n", label.len, label.data);
        return -1;
    }

    return 0;
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

static int
create_fh_ast(struct ast* ast, const struct YYLTYPE* loc)
{
    int jump_f, jump_b;

    /* hash40("jump_f") = 0x62dd02058 */
    jump_f = ast_motion(ast, 0x62dd02058ul, loc);
    if (jump_f < 0) return -1;

    /* hash40("jump_b") = 0x62abde441 */
    jump_b = ast_motion(ast, 0x62abde441ul, loc);
    if (jump_b < 0) return -1;

    /* jump_f | jump_b */
    return ast_union(ast, jump_f, jump_b, loc);
}

static int
create_sh_ast(struct ast* ast, const struct YYLTYPE* loc)
{
    int jump_f_mini, jump_b_mini;

    /* hash40("jump_f_mini") = 0xb38c9ab48 */
    jump_f_mini = ast_motion(ast, 0xb38c9ab48ul, loc);
    if (jump_f_mini < 0) return -1;

    /* hash40("jump_b_mini") = 0xba358e95e */
    jump_b_mini = ast_motion(ast, 0xba358e95eul, loc);
    if (jump_b_mini < 0) return -1;

    /* jump_f_mini | jump_b_mini */
    return ast_union(ast, jump_f_mini, jump_b_mini, loc);
}

static int
create_dj_ast(struct ast* ast, const struct YYLTYPE* loc)
{
    int jump_aerial_f, jump_aerial_b;

    /* hash40("jump_aerial_f") = 0xd0b71815b */
    jump_aerial_f = ast_motion(ast, 0xd0b71815bul, loc);
    if (jump_aerial_f < 0) return -1;

    /* hash40("jump_aerial_b") = 0xd0c1c4542 */
    jump_aerial_b = ast_motion(ast, 0xd0c1c4542ul, loc);
    if (jump_aerial_b < 0) return -1;

    /* jump_aerial_f | jump_aerial_b */
    return ast_union(ast, jump_aerial_f, jump_aerial_b, loc);
}

static int
create_fs_ast(struct ast* ast, const struct YYLTYPE* loc)
{
    return -1;
}

static int
create_idj_ast(struct ast* ast, const struct YYLTYPE* loc)
{
    int jump_squat;
    int jump_f, jump_b;
    int jump_f_mini, jump_b_mini;
    int jump_aerial_f, jump_aerial_b;
    int sh, fh, jump, dj, idj;

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
    dj = ast_timing(ast, -1, dj, 1, 1, loc);
    if (dj < 0) return -1;

    idj = ast_statement(ast, jump_squat, jump, loc);
    if (idj < 0) return -1;
    return ast_statement(ast, idj, dj, loc);
}

int
ast_post_jump_qualifiers(struct ast* ast)
{
    int n, j;

    struct entry {
        enum ast_ctx_flags flag;
        int (*create_ast)(struct ast* ast, const struct YYLTYPE* loc);
    };

    struct entry jumps[] = {
        { AST_CTX_FH, create_fh_ast },
        { AST_CTX_SH, create_sh_ast },
        { AST_CTX_DJ, create_dj_ast },
        { AST_CTX_IDJ, create_idj_ast },
        { AST_CTX_FS, create_fs_ast }
    };

    for (n = 0; n != ast->node_count; ++n)
        for (j = 0; j != sizeof(jumps) / sizeof(*jumps); ++j)
        {
            int jump;
            const struct YYLTYPE* loc = (const struct YYLTYPE*)&ast->nodes[n].info.loc;

            /* The node type can change */
            if (ast->nodes[n].info.type != AST_CONTEXT)
                break;
            if (!(ast->nodes[n].context.flags & jumps[j].flag))
                continue;

            jump = jumps[j].create_ast(ast, loc);
            if (jump < 0)
                return -1;

            /*
             * The jump flag is grouped together with other context flags,
             * so the original node may still need to be kept around if this
             * wasn't the only flag.
             */
            ast->nodes[n].context.flags &= ~jumps[j].flag;
            if (ast->nodes[n].context.flags == 0)
            {
                int stmt = ast_statement(ast, jump, ast->nodes[n].context.child, loc);
                if (stmt < 0) return -1;
                ast_collapse_into(ast, stmt, n);
            }
            else
            {
                int parent = ast_find_parent(ast, n);
                /* NOTE: Have to find parent before setting "dj" to be the new parent */
                int stmt = ast_statement(ast, jump, n, loc);
                if (stmt < 0) return -1;
                if (parent < 0)
                    ast_set_root(ast, stmt);
                else
                {
                    if (ast->nodes[parent].base.left == n)
                        ast->nodes[parent].base.left = stmt;
                    if (ast->nodes[parent].base.right == n)
                        ast->nodes[parent].base.right = stmt;
                }
            }
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
            if (ast->nodes[n].timing.rel_to_ref >= 0)
                continue;

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

                ast->nodes[n].timing.rel_to_ref = prev_stmt;
            }
            else
            {
                /*
                 * Iterate over all other subtrees in the ast that match the
                 * "rel_to" subtree. There could be multiple matches. The goal
                 * is to find a match occuring as "early" as possible.
                 */
                int n2, stmt = -1;
                for (n2 = 0; n2 != ast->node_count; ++n2)
                {
                    if (ast_is_in_subtree_of(ast, n2, n))
                        continue;
                    if (ast_trees_equal(ast, ast->nodes[n].timing.rel_to, ast, n2))
                        if (ast_node_preceeds(ast, n2, stmt == -1 ? n : stmt))
                            stmt = n2;
                }

                if (stmt == -1)
                {
                    log_err("Failed to find timing reference\n");
                    return -1;
                }

                ast->nodes[n].timing.rel_to_ref = stmt;
            }
        }
    return 0;
}

static void
ast_post_damage_tighten_bounds(struct ast* ast, int parent)
{
    /* Travel down chain of damage nodes */
    int child = parent;
    while (1)
    {
        float cfrom, cto;
        float pfrom = ast->nodes[parent].damage.from;
        float pto = ast->nodes[parent].damage.to;

        child = ast->nodes[child].damage.child;
        if (ast->nodes[child].info.type != AST_DAMAGE)
            break;
        cfrom = ast->nodes[child].damage.from;
        cto = ast->nodes[child].damage.to;

        /*
         * If the parent has specified ">x", then all children's lower bounds
         * will need to be greater than x as well. Conversely, if the parent
         * has specified "<x" then all children's upper bounds will need to be
         * smaller than x as well.
         */
        if (pto >= 999.f && cfrom > 0.f && cfrom < pfrom)
            ast->nodes[child].damage.from = pfrom;
        if (pfrom <= 0.f && cto < 999.f && cto > pto)
            ast->nodes[child].damage.to = pto;

        /* Same, but with child and parent swapped */
        if (cto >= 999.f && pfrom > 0.f && pfrom < cfrom)
            ast->nodes[parent].damage.from = cfrom;
        if (cfrom <= 0.f && pto < 999.f && pto > cto)
            ast->nodes[parent].damage.to = cto;
    }
}

static int
ast_post_damage_merge_nodes(struct ast* ast, int parent)
{
    /* Travel down chain of damage nodes */
    int child = parent;
    while (1)
    {
        float from1, from2, to1, to2, from, to;
        child = ast->nodes[child].damage.child;
        if (ast->nodes[child].info.type != AST_DAMAGE)
            break;
        if (ast->nodes[child].damage.from > ast->nodes[child].damage.to)
        {
            log_err("Damage range invalid (%.1f%% - %.1f%%)\n",
                ast->nodes[child].damage.from, ast->nodes[child].damage.to);
            return -1;
        }

        /* Merge overlapping damage ranges, if possible */
        from1 = ast->nodes[parent].damage.from;
        from2 = ast->nodes[child].damage.from;
        to1 = ast->nodes[parent].damage.to;
        to2 = ast->nodes[child].damage.to;
        from = from1 > from2 ? from1 : from2;
        to = to1 < to2 ? to1 : to2;
        if (from < to)
        {
            ast->nodes[child].damage.from = from;
            ast->nodes[child].damage.to = to;
            ast_swap_node_values(ast, child, ast->nodes[parent].damage.child);
            ast_collapse_into(ast, ast->nodes[parent].damage.child, parent);
            return 1;
        }
    }

    return 0;
}

int
ast_post_damage(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
    {
        if (ast->nodes[n].info.type != AST_DAMAGE)
            continue;
        ast_post_damage_tighten_bounds(ast, n);
    }

retry:
    for (n = 0; n != ast->node_count; ++n)
    {
        if (ast->nodes[n].info.type != AST_DAMAGE)
            continue;

        /* Ensure the ranges are valid */
        if (ast->nodes[n].damage.from > ast->nodes[n].damage.to)
        {
            log_err("Damage range invalid (%.1f%% - %.1f%%)\n",
                ast->nodes[n].damage.from, ast->nodes[n].damage.to);
            return -1;
        }

        switch (ast_post_damage_merge_nodes(ast, n))
        {
            case 1  : goto retry;
            case 0  : break;
            default : return -1;
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

            case AST_CONTEXT: {
                const enum ast_ctx_flags invalid_flags =
                    AST_CTX_OOS |
                    AST_CTX_SH |
                    AST_CTX_FH |
                    AST_CTX_DJ |
                    AST_CTX_FS |
                    AST_CTX_IDJ |
                    AST_CTX_KILL |
                    AST_CTX_DIE;
                if (ast->nodes[n].context.flags & invalid_flags)
                {
                    log_err("Invalid flags set in context qualifiers! Should not happen\n");
                    return -1;
                }
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

                if (ast->nodes[n].timing.rel_to_ref < 0)
                {
                    log_err("Timing has no reference!\n");
                    return -1;
                }
                if (ast_is_in_subtree_of(ast, n, ast->nodes[n].timing.rel_to_ref))
                {
                    log_err("Invalid timing reference\n");
                    return -1;
                }
            } break;

            case AST_DAMAGE: {
                float from = ast->nodes[n].damage.from;
                float to = ast->nodes[n].damage.to;

                if (from > to)
                {
                    log_err("Damage range invalid (%.1f%% - %.1f%%)\n", from, to);
                    return -1;
                }
            } break;

            default: break;
        }

    return 0;
}
