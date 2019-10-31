#ifndef POET_CODE_POET_COMMON_DEFINITIONS_H
#define POET_CODE_POET_COMMON_DEFINITIONS_H

#ifdef DEBUG
#define ERR(...) do {fprintf(stderr, __VA_ARGS__);} while(0)
#define ERRR(...) do {fprintf(stderr, "[%s:%d] ", (strrchr(__FILE__, '/') != NULL ? strrchr(__FILE__, '/')+1 : __FILE__), __LINE__); \
                      fprintf(stderr, __VA_ARGS__);} while(0)
#else
#define ERR(...) /**/
#define ERRR(...) /**/
#endif

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
