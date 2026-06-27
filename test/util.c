#include <errno.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include "util.h"
#include "test.h"

void test_normalize_path(void);
void test_path_next_dir(void);
void test_create_directories_from_path(void);

void test_util(void)
{
    test_normalize_path();
    test_path_next_dir();
    test_create_directories_from_path();
}

void test_normalize_path(void)
{
    char* input[] = {
        "/usr/./hello/world.txt",
        "/usr/../hello/world.txt",
    };
    char* expected[] = {
        "/usr/hello/world.txt",
        "/hello/world.txt",
    };

    for (size_t i = 0; i < sizeof(input) / sizeof(input[0]); i++)
    {
        char* norm = normalize_path(input[i]);
        if (streq(norm, expected[i]))
        {
            PASS("'%s' = '%s'", norm, expected[i]);
        }
        else
        {
            FAIL("'%s' != '%s'", norm, expected[i]);
        }
    }
}

void test_path_next_dir(void)
{
    char* root_dir = "/usr/share";
    char* path = "/usr/share/dict/british";
    int root_dir_len = strlen(root_dir);
    char* dir = path_next_dir(path, root_dir_len - 1);

    if (streq(dir, "/usr/share"))
    {
        PASS("'%s'", dir);
    }
    else
    {
        FAIL("'%s'", dir);
    }

    dir = path_next_dir(NULL, 0);
    if (streq(dir, "/usr/share/dict"))
    {
        PASS("'%s'", dir);
    }
    else
    {
        FAIL("'%s'", dir);
    }
}

void test_create_directories_from_path(void)
{
    char filename[] = "hello/world/file.txt\0";
    char* dirs = dirname(filename);
    int res = create_directories_from_path("/tmp", dirs);

    if (res == 0)
    {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "/tmp/%s", dirs);
        if (rmdir(full_path) == -1)
        {
            if (errno == ENOENT)
            {
                FAIL("directory '%s' was not created", full_path);
                return;
            }
            else
            {
                WARN("error while removing directory to test existence D:");
            }
        }

        PASS("directory '%s' was created", full_path);
    }
    else
    {
        FAIL("returned %d", res);
    }
}
