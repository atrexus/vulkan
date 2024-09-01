#include <stdio.h>

#define error(fmt, ...) fprintf(stderr, "vulkan [err] " fmt "\n", __VA_ARGS__)
#define info(fmt, ...) fprintf(stdout, "vulkan [inf] " fmt "\n", __VA_ARGS__)
#define warn(fmt, ...) fprintf(stdout, "vulkan [wrn] " fmt "\n", __VA_ARGS__)

#ifdef NDEBUG
#    define debug(fmt, ...)
#else
#    define debug(fmt, ...) fprintf(stdout, "vulkan [dbg] " fmt "\n", __VA_ARGS__)
#endif // DEBUG