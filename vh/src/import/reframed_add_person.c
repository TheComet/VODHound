#include "vh/db.h"

int
reframed_add_person_to_db(
    struct db_interface* dbi, struct db* db,
    int sponsor_id,
    struct str_view name, struct str_view tag,
    struct str_view social, struct str_view pronouns)
{
    int person_id = dbi->person.add_or_get(db,
        sponsor_id, name, tag, social, pronouns);
    if (person_id < 0)
        return -1;
    if (dbi->person.merge_social_and_pronouns(db, person_id, social, pronouns) < 0)
        return -1;

    return person_id;
}
