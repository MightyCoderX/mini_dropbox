#ifndef _TEST_H
#define _TEST_H

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"

#define FAIL(fmt, ...) fprintf(stderr, RED "[Failed] %s " fmt RESET "\n", __func__, ##__VA_ARGS__)
#define PASS(fmt, ...) fprintf(stderr, GREEN "[Passed] %s " fmt RESET "\n", __func__, ##__VA_ARGS__)

#define WARN(fmt, ...) fprintf(stderr, YELLOW "[Warn] %s " fmt RESET, __func__, ##__VA_ARGS__)

#define streq(str1, str2) (strcmp(str1, str2) == 0)

#endif // !_TEST_H
