/**
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 43):
 *
 * GitHub Co-pilot and <jens@bennerhq.com> wrote this file.  As long as you 
 * retain this notice you can do whatever you want with this stuff. If we meet 
 * some day, and you think this stuff is worth it, you can buy me a beer in 
 * return.   
 *
 * /benner
 * ----------------------------------------------------------------------------
 */

#ifndef __DMALLOC_H__
#define __DMALLOC_H__

#define MALLOC_DEBUG       (1)
#define MALLOC_TRACKING    (0)
#define MALLOC_CALLSTACK   (1)
#define MALLOC_HEXDUMP     (1)

#if MALLOC_DEBUG
    void *debug_malloc(size_t size, const char *file, int line);
    void debug_free(void *ptr, const char *file, int line);
    void *debug_realloc(void* ptr, size_t size, size_t old_size, const char *file, int line);
    void debug_cleaning(const char *file, int line);
    void debug_integrity(const char *file, int line);

#   define mem_malloc(size)                     debug_malloc(size, __FILE__, __LINE__)
#   define mem_free(ptr)                        debug_free(ptr, __FILE__, __LINE__)
#   define mem_realloc(ptr, size, old_size)     debug_realloc(ptr, size, old_size, __FILE__, __LINE__)
#   define mem_cleaning()                       debug_cleaning(__FILE__, __LINE__)
#   define mem_integrate()                      debug_integrity(__FILE__, __LINE__)
#else
#   define mem_malloc(size)                     malloc(size)
#   define mem_free(ptr)                        free(ptr)
#   define mem_realloc(ptr, size, old_size)     realloc(ptr, size)
#   define mem_cleaning()                       {}
#   define mem_integrate()                      {}
#endif

#endif /* __DMALLOC_H__ */
