#include "vh/db.h"

struct person_add_or_get_ctx
{
    struct db_interface* dbi;
    struct db* db;
    struct str_view name;
    struct str_view tag;
    struct str_view social;
    struct str_view pronouns;
};

static int
on_person_add(int id, int sponsor_id, const char* name, const char* tag, const char* social, const char* pronouns, void* user)
{
    struct person_add_or_get_ctx* ctx = user;
    (void)sponsor_id;  (void)name; (void)tag;

    /*
     * If social or pronouns are empty, but we have read non-empty strings
     * from the rfr replay, then update those.
     */
    if (!*social && ctx->social.len)
        if (ctx->dbi->person.set_social(ctx->db, id, ctx->social) < 0)
            return -1;
    if (!*pronouns && ctx->pronouns.len)
        if (ctx->dbi->person.set_pronouns(ctx->db, id, ctx->pronouns) < 0)
            return -1;

    return 0;
}

int
reframed_add_person_to_db(
    struct db_interface* dbi, struct db* db,
    int sponsor_id,
    struct str_view name, struct str_view tag,
    struct str_view social, struct str_view pronouns)
{
    struct person_add_or_get_ctx ctx = {
        dbi, db,
        name, tag, social, pronouns
    };
    int person_id = dbi->person.add_or_get(db,
        sponsor_id, name, tag, social, pronouns, on_person_add, &ctx);
    return person_id;
}
