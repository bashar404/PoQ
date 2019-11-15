#ifndef POET_CODE_POET_COMMON_DEFINITIONS_H
#define POET_CODE_POET_COMMON_DEFINITIONS_H

/* E --> ERROR
 * ER -> WARNING
 * ERR -> First level DEBUG
 * ERRR -> Second level DEBUG
 * INFO -> Informative
 * */

#ifndef __cplusplus
#include <string.h>
#include <pthread.h>
#include <assert.h>
#else
#include <pthread.h>
#include <cstring>
#include <cassert>
#endif

#ifdef DEBUG
#define ERRR(...) do {fprintf(stderr, "DEBUG2  [0x%lx|%s:%3d] ", pthread_self(), (strrchr(__FILE__, '/') != NULL ? strrchr(__FILE__, '/')+1 : __FILE__), __LINE__); \
                      fprintf(stderr, __VA_ARGS__);} while(0)
#define DEBUG1
#else
#define ERRR(...) /**/
#endif

#ifdef DEBUG1
#define ERR(...) do {fprintf(stderr, "DEBUG1  [0x%lx|%s:%3d] ", pthread_self(), (strrchr(__FILE__, '/') != NULL ? strrchr(__FILE__, '/')+1 : __FILE__), __LINE__); \
                      fprintf(stderr, __VA_ARGS__);} while(0)
#define WARNING
#ifndef DEBUG
#define DEBUG
#endif
#else
#define ERR(...) /**/
#endif

#ifdef WARNING
#define ER(...) do {fprintf(stderr, "WARNING [0x%lx|%s:%3d] ", pthread_self(), (strrchr(__FILE__, '/') != NULL ? strrchr(__FILE__, '/')+1 : __FILE__), __LINE__); \
                      fprintf(stderr, __VA_ARGS__);} while(0)
#ifndef DEBUG
#define DEBUG
#endif
#else
#define ER(...) /**/
#endif

#define E(...) do {fprintf(stderr, "ERROR   [0x%lx|%s:%3d] ", pthread_self(), (strrchr(__FILE__, '/') != NULL ? strrchr(__FILE__, '/')+1 : __FILE__), __LINE__); \
                      fprintf(stderr, __VA_ARGS__);} while(0)

#define INFO(...) do {fprintf(stderr, "INFO    [0x%lx|%s:%3d] ", pthread_self(), (strrchr(__FILE__, '/') != NULL ? strrchr(__FILE__, '/')+1 : __FILE__), __LINE__); \
                      fprintf(stderr, __VA_ARGS__);} while(0)


#define assertp(expr) do { int _val = (expr); if (!_val) { \
                           ((!errno) ? __assert_fail (#expr, __FILE__, __LINE__, __ASSERT_FUNCTION) : __assert_perror_fail ((errno), __FILE__, __LINE__, __ASSERT_FUNCTION)); \
                           exit(EXIT_FAILURE); } } while(0)

#ifndef __cplusplus
# ifndef max
#  define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#  define min(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

# endif
#endif

#define BUFFER_SIZE 1024

#endif //POET_CODE_POET_COMMON_DEFINITIONS_H
