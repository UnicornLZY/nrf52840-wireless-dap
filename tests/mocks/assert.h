#pragma once
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef assert
#define assert(expr) \
  do { \
    if (!(expr)) { \
      fprintf(stderr, "\nAssertion failed: %s, file %s, line %d\n", #expr, __FILE__, __LINE__); \
      fflush(stderr); \
      exit(1); \
    } \
  } while (0)

#ifdef __cplusplus
}
#endif
