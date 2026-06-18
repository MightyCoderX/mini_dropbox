#include <stdio.h>
#include <libgen.h>

#include "util.h"

int test_util(void)
{
    char filename[] = "hello/world/file.txt\0";
    char* dirs = dirname(filename);
    printf("calling create_directories_from_path:\n");
    int res = create_directories_from_path("/tmp", dirs);

    printf("res: %d\n", res);

    return 0;
}
