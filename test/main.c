#include <stdio.h>

#include "util.c"

int main(void)
{
    printf("----------TESTS Begin----------\n");

    if (test_util() >= 0)
    {
        printf("TEST passed: util\n");
    }
    else
    {
        printf("TEST failed: util\n");
    }

    printf("\n----------TESTS End----------\n");

    return 0;
}
