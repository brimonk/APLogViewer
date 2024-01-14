#ifndef COMMON_H_
#define COMMON_H_

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#ifdef __cplusplus
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <mutex>
#endif // __cplusplus

#include <windows.h>

typedef int8_t     i8;
typedef int16_t    i16;
typedef int32_t    i32;
typedef int64_t    i64;
typedef uint8_t    u8;
typedef uint16_t   u16;
typedef uint32_t   u32;
typedef uint64_t   u64;
typedef float      f32;
typedef double     f64;

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define ARRSIZE(x) (sizeof((x))/sizeof((x)[0]))
#define SWAP(a, b) do { typeof((a)) z_ = (b); (b) = (a); (a) = z_; } while (0);

#define streq(a, b) (strcmp((a), (b)) == 0 && strlen((a)) == strlen((b)))

#define ERR(fmt, ...)  fprintf(stderr, "ERR: " fmt "\n", __VA_ARGS__)
#define LOG(fmt, ...)  fprintf(stderr, "LOG: " fmt "\n", __VA_ARGS__)

#endif // COMMON_H_
