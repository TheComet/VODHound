#include <gmock/gmock.h>
#include "vh/fs.h"

#define NAME vh_fs

using namespace testing;

struct NAME : public Test
{
    void SetUp() override
    {
        path_init(&path);
    }
    void TearDown() override
    {
        path_deinit(&path);
    }

    struct path path;
};

TEST_F(NAME, path_set_empty)
{
    path_set(&path, cstr_view(""));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq(""));
}

TEST_F(NAME, path_set_replace_slashes)
{
#ifdef _WIN32
    path_set(&path, cstr_view("some/unix/path"));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some\\unix\\path"));
#else
    path_set(&path, cstr_view("some\\windows\\path"));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some/windows/path"));
#endif
}

TEST_F(NAME, path_set_remove_trailing_slashes)
{
#ifdef _WIN32
    path_set(&path, cstr_view("some/unix/path/"));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some\\unix\\path"));
#else
    path_set(&path, cstr_view("some\\windows\\path\\"));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some/windows/path"));
#endif
}

TEST_F(NAME, path_join_empty)
{
    path_set(&path, cstr_view(""));
    path_join(&path, cstr_view(""));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq(""));
}

TEST_F(NAME, path_join_1)
{
#ifdef _WIN32
    path_set(&path, cstr_view("some/"));
    path_join(&path, cstr_view("unix/path/"));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some\\unix\\path"));
#else
    path_set(&path, cstr_view("some\\"));
    path_join(&path, cstr_view("windows\\path\\"));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some/windows/path"));
#endif
}

TEST_F(NAME, path_join_2)
{
#ifdef _WIN32
    path_set(&path, cstr_view("some"));
    path_join(&path, cstr_view("unix/path"));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some\\unix\\path"));
#else
    path_set(&path, cstr_view("some"));
    path_join(&path, cstr_view("windows\\path"));
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some/windows/path"));
#endif
}

TEST_F(NAME, path_dirname_empty)
{
    path_set(&path, cstr_view(""));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq(""));
}

TEST_F(NAME, path_dirname_file_1)
{
    path_set(&path, cstr_view("file.dat.xz"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq(""));
}

TEST_F(NAME, path_dirname_on_file_2)
{
#ifdef _WIN32
    path_set(&path, cstr_view("/file.dat.xz"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq(""));
#else
    path_set(&path, cstr_view("\\file.dat.xz"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("/"));
#endif
}

TEST_F(NAME, path_dirname_on_file_3)
{
#ifdef _WIN32
    path_set(&path, cstr_view("some/unix/file.dat.xz"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some\\unix"));
#else
    path_set(&path, cstr_view("some\\windows\\file.dat.xz"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some/windows"));
#endif
}

TEST_F(NAME, path_dirname_on_path_1)
{
#ifdef _WIN32
    path_set(&path, cstr_view("some/unix/path"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some\\unix"));
#else
    path_set(&path, cstr_view("some\\windows\\path"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some/windows"));
#endif
}

TEST_F(NAME, path_dirname_on_path_2)
{
#ifdef _WIN32
    path_set(&path, cstr_view("some/unix/path/"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some\\unix"));
#else
    path_set(&path, cstr_view("some\\windows\\path\\"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("some/windows"));
#endif
}

TEST_F(NAME, path_dirname_on_path_3)
{
#ifdef _WIN32
    path_set(&path, cstr_view("/some/unix/path"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq(""));
#else
    path_set(&path, cstr_view("\\some\\windows\\path"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq("/some/windows"));
#endif
}

TEST_F(NAME, path_dirname_on_path_4)
{
    path_set(&path, cstr_view("dir"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq(""));
}

TEST_F(NAME, path_dirname_on_path_5)
{
#ifdef _WIN32
    path_set(&path, cstr_view("/"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq(""));
#else
    path_set(&path, cstr_view("\\"));
    path_dirname(&path);
    path_terminate(&path);
    EXPECT_THAT(path.str.data, StrEq(""));
#endif
}
