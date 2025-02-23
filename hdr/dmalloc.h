
/**
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 43):
 *
 * ChatGPT and <jens@bennerhq.com> wrote this file.  As long as you retain this
 * notice you can do whatever you want with this stuff. If we meet some day, and 
 * you think this stuff is worth it, you can buy me a beer in return.   
 *
 * /benner
 * ----------------------------------------------------------------------------
 */

#ifndef __DMALLOC_H__

#define MALLOC_DEBUG       (1)
#define MALLOC_TRACKING    (0)

#if MALLOC_DEBUG == 1
    void *debug_malloc(size_t size, const char *file, int line);
    void debug_free(void *ptr, const char *file, int line);
    void debug_memory_leaks(const char *file, int line);

#   define mem_alloc(size)     debug_malloc(size, __FILE__, __LINE__)
#   define mem_free(ptr)       debug_free(ptr, __FILE__, __LINE__)
#   define mem_check()         debug_memory_leaks(__FILE__, __LINE__)

#else
#   define mem_alloc(size)     malloc(size)
#   define mem_free(ptr)       free(ptr)
#   define mem_check()         {}
#endif

#endif /* __DMALLOC_H__ */
