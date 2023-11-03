#include "search/ast.h"
#include "search/ast_post.h"

#include "vh/db_ops.h"
#include "vh/hm.h"
#include "vh/mem.h"
#include "vh/str.h"
#include "vh/vec.h"

int
ast_post_patch_motions(struct ast* ast, struct db_interface* dbi, struct db* db, int fighter_id, struct hm* original_labels)
{
    int n, root;
    struct vec motions;
    vec_init(&motions, sizeof(uint64_t));

    for (n = 0; n != ast->node_count; ++n)
    {
        struct str_view label;
        if (ast->nodes[n].info.type != AST_LABEL)
            continue;

        label = cstr_view(ast->nodes[n].labels.label);

        /*
         * If the label is a user-defined label, for example "nair", then it
         * may map to more than 1 motion value, for example, "attack_air_n"
         * and "landing_air_n". The label node in the AST is replaced with
         * "(attack_air_n|landing_air_n)+". This will match all patterns of
         * "nair".
         */
        vec_clear(&motions);
        switch (dbi->motion_label.to_motions(db, fighter_id, label, &motions))
        {
            case 1  :
                root = -1;
                VEC_FOR_EACH(&motions, uint64_t, motion)
                    char** plabel;
                    int node = ast_motion(ast, *motion, (struct YYLTYPE*)&ast->nodes[n].info.loc);
                    if (node < 0)
                        goto error;

                    root = root == -1 ? node : ast_union(ast, root, node, (struct YYLTYPE*)&ast->nodes[n].info.loc);
                    if (root < 0)
                        goto error;

                    switch (hm_insert(original_labels, motion, (void**)&plabel))
                    {
                        case 1:
                            *plabel = mem_alloc(label.len + 1);
                            if (*plabel == NULL)
                                goto error;
                            strcpy(*plabel, label.data);
                            break;
                        case 0: break;
                        default: goto error;
                    }
                VEC_END_EACH

                if (root >= 0)
                {
                    root = ast_repetition(ast, root, 1, -1, (struct YYLTYPE*)&ast->nodes[n].info.loc);
                    if (root < 0)
                        goto error;
                    ast_collapse_into(ast, root, n);
                }

                break;

            case 0  : break;
            default : goto error;
        }
    }

    vec_deinit(&motions);
    return 0;

error:
    vec_deinit(&motions);
    return -1;
}
