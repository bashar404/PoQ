#ifndef POET_CODE_POET_COMMON_DEFINITIONS_H
#define POET_CODE_POET_COMMON_DEFINITIONS_H

/* ERROR --> ERROR
 * WARN -> WARNING
 * ERR -> First level DEBUG
 * ERRR -> Second level DEBUG
 * INFO -> Informative
 * */

#ifndef __cplusplus
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <stdlib.h>
#else
#include <pthread.h>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <cerrno>
#include <semaphore.h>
#include <cstdlib>
#endif

#define BUFFER_SIZE 1024

#define __STDERR_MSG(out, type, format, ...) do {fprintf(out, type " [0x%lx|%s:%3d] " format, pthread_self(), (strrchr(__FILE__, '/') != NULL ? strrchr(__FILE__, '/')+1 : __FILE__), __LINE__, \
                                            ##__VA_ARGS__);} while(0)

#define __GENERAL_MSG(out, type, format, ...) do {fprintf(out, type " " format, ##__VA_ARGS__);} while(0)

#ifdef DEBUG
#define ERRR(...) __STDERR_MSG(stdout, "DEBUG2 ", __VA_ARGS__)
#define DEBUG1
#else
#define ERRR(...) /**/
#endif

#ifdef DEBUG1
#define ERR(...) __STDERR_MSG(stdout, "DEBUG1 ", __VA_ARGS__)
#define WARNING
#ifndef DEBUG
#define DEBUG
#endif
#else
#define ERR(...) /**/
#endif

#ifdef WARNING
#define WARN(...) __STDERR_MSG(stderr, "WARNING",  __VA_ARGS__)
#ifndef DEBUG
#define DEBUG
#endif
#else
#define WARN(...) /**/
#endif

#define ERROR(...) __STDERR_MSG(stderr, "ERROR  ",  __VA_ARGS__)

#define INFO(...) __STDERR_MSG(stdout, "INFO   ", __VA_ARGS__)

#define STR(X) (#X)

#ifndef NDEBUG
#define assertp(expr) do { int _val = (expr); if (!_val) { \
                           ((!errno) ? __assert_fail (#expr, __FILE__, __LINE__, __ASSERT_FUNCTION) : __assert_perror_fail ((errno), __FILE__, __LINE__, __ASSERT_FUNCTION)); \
                           exit(EXIT_FAILURE); } } while(0)
#else
#define assertp(expr) do { int _val = (expr); if (!_val) {((!errno) ? exit(EXIT_FAILURE) : exit(errno));} } while(0)
#endif

FILE *__get_log_file(int);
sem_t *__get_logfile_semaphore(int);

#define REPORT(...) \
do { \
    int oldstate;\
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);\
    sem_t *sem = __get_logfile_semaphore(0);\
    sem_wait(sem);\
    FILE* out = __get_log_file(0); \
    __GENERAL_MSG(out, "REPORT ", __VA_ARGS__);\
    fflush(out);\
    sem_post(sem);\
    pthread_setcancelstate(oldstate, &oldstate);\
} while(0)


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

#endif //POET_CODE_POET_COMMON_DEFINITIONS_H
