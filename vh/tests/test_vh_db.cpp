#include "gmock/gmock.h"
#include "vh/db.h"

#define NAME vh_db

using namespace testing;

struct NAME : Test
{
    void SetUp() override
    {
        dbi = ::db("sqlite3");
        db = dbi->open("test.db");
    }

    void TearDown() override
    {
        dbi->close(db);
    }

    struct db_interface* dbi;
    struct db* db;
};

TEST_F(NAME, motions)
{
    EXPECT_THAT(dbi->motion.add(db, 0x1234, cstr_view("test")), Eq(0));
    EXPECT_THAT(dbi->motion.add(db, 0x1235, cstr_view("foo")), Eq(0));
    EXPECT_THAT(dbi->motion.exists(db, 0x1234), IsTrue());
    EXPECT_THAT(dbi->motion.exists(db, 0x1235), IsTrue());
    EXPECT_THAT(dbi->motion.exists(db, 0x1236), IsFalse());
}
