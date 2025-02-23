/**
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 43):
 *
 * ChatGPT wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   
 *
 * /benner
 * ----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include "../hdr/dmalloc.h" 

#define COLOR_RED       "\033[31m"
#define COLOR_RESET     "\033[0m"

// Debugging functionality to track memory allocations
typedef struct MemTrack {
    void *ptr;
    const char *file;
    int line;
    struct MemTrack *next;
} MemTrack;

MemTrack *mem_track_head = NULL;

void *debug_malloc(size_t size, const char *file, int line) {
    void *ptr = malloc(size);
    if (ptr) {
        MemTrack *new_track = (MemTrack *)malloc(sizeof(MemTrack));
        new_track->ptr = ptr;
        new_track->file = file;
        new_track->line = line;
        new_track->next = mem_track_head;
        mem_track_head = new_track;

#ifdef MALLOC_TRACKING
        fprintf(stderr, COLOR_RED "malloc %p at %s:%d\n" COLOR_RESET, ptr, file, line);
#endif
    }
    return ptr;
}

void debug_free(void *ptr, const char *file, int line) {
    MemTrack **current = &mem_track_head;
    while (*current) {
        if ((*current)->ptr == ptr) {
            MemTrack *to_free = *current;
            *current = (*current)->next;
            free(to_free);
#ifdef MALLOC_TRACKING
            fprintf(stderr, COLOR_RED "mfree %p at %s:%d\n" COLOR_RESET, ptr, file, line);
#endif
                break;
        }
        current = &(*current)->next;
    }
    free(ptr);
}

void debug_memory_leaks() {
    MemTrack *current = mem_track_head;
    while (current) {
        fprintf(stderr, "Memory leak detected: %p allocated at %s:%d\n", current->ptr, current->file, current->line);
        current = current->next;
    }
}
