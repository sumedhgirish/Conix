#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <string.h>

#define CONIX_GLOBAL_PREFIX "/home/crownedhog/.conix"

#define ERROR "\x1b[31mERROR\x1b[0m"
#define WARN "\x1b[33mWARN \x1b[0m"
#define DEBUG "\x1b[36mDEBUG\x1b[0m"
#define INFO "\x1b[32mINFO \x1b[0m"

#define __FILENAME__                                                           \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG(level, fmt, ...)                                                   \
    do                                                                         \
    {                                                                          \
        fprintf(stderr, "[%5s] <%-10s> " fmt "\n", level, __FILENAME__,        \
                ##__VA_ARGS__);                                                \
    } while (0)

#endif
