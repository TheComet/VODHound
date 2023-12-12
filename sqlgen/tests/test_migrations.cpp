#include <gmock/gmock.h>
#include "sqlgen/tests/migrations.h"

#define NAME sqlgen_migrations

using namespace testing;

struct NAME : public Test
{
    void SetUp() override {
        dbi = migrations("sqlite3");
        db = dbi->open("migrations.db");
    }

    void TearDown() override {
        dbi->close(db);
    }

    struct migrations_interface* dbi;
    struct migrations* db;
};

TEST_F(NAME, test)
{

}
