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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../hdr/dmalloc.h" 

#define COLOR_RED       "\033[31m"
#define COLOR_GREEN     "\033[32m"
#define COLOR_RESET     "\033[0m"

#define HEAD_MAGIC      "HEAD_MAGIC"
#define TAIL_MAGIC      "TAIL_MAGIC"
#define HEAD_MAGIC_SIZE (sizeof(HEAD_MAGIC))
#define TAIL_MAGIC_SIZE (sizeof(TAIL_MAGIC))

typedef struct MemTrack {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    struct MemTrack *next;
} MemTrack;

MemTrack *mem_track_head = NULL;

MemTrack * debug_find_ptr(void *ptr, const char *file, int line) {
    MemTrack *current = mem_track_head;
    while (current) {
        if (current->ptr == ptr) {
            return current;
        }
        current = current->next;
    }

    fprintf(stderr, COLOR_RED "Memory corruption detected. Can't find %p allocated at %s:%d\n" COLOR_RESET, ptr, file, line);
    return NULL;
}

void debug_check_ptr(void *user_ptr, const char *file, int line) {
    if (!user_ptr) {
        fprintf(stderr, COLOR_RED "Memory corrupted as NULL pointer detected\n" COLOR_RESET);
        exit(EXIT_FAILURE);
    }

    char *ptr = (char *)user_ptr - HEAD_MAGIC_SIZE;

    MemTrack *track = debug_find_ptr(ptr, file, line);
    if (!track) return;

    if (memcmp(ptr, HEAD_MAGIC, HEAD_MAGIC_SIZE) != 0) {
        fprintf(stderr, COLOR_RED "Memory corruption detected at head of %p allocated at %s:%d\n" COLOR_RESET, user_ptr, file, line);
        fprintf(stderr, COLOR_RED "Memory allocated at: %s at %d\n" COLOR_RESET, track->file, track->line);
    }
    if (memcmp(ptr + HEAD_MAGIC_SIZE + track->size, TAIL_MAGIC, TAIL_MAGIC_SIZE) != 0) {
        fprintf(stderr, COLOR_RED "Memory corruption detected at tail of %p allocated at %s:%d\n" COLOR_RESET, user_ptr, file, line);
        fprintf(stderr, COLOR_RED "Memory allocated at: %s at %d\n" COLOR_RESET, track->file, track->line);
    }
}

void *debug_malloc(size_t size, const char *file, int line) {
    void *ptr = malloc(size + HEAD_MAGIC_SIZE + TAIL_MAGIC_SIZE);
    if (ptr) {
        MemTrack *new_track = (MemTrack *)malloc(sizeof(MemTrack));
        new_track->ptr = ptr;
        new_track->size = size;
        new_track->file = file;
        new_track->line = line;
        new_track->next = mem_track_head;
        mem_track_head = new_track;

        memcpy(ptr, HEAD_MAGIC, HEAD_MAGIC_SIZE);
        memcpy((char *)ptr + HEAD_MAGIC_SIZE + size, TAIL_MAGIC, TAIL_MAGIC_SIZE);
        ptr += HEAD_MAGIC_SIZE;

        debug_check_ptr(ptr, file, line);

#if MALLOC_TRACKING
        fprintf(stderr, COLOR_GREEN "malloc %p at %s:%d\n" COLOR_RESET, ptr, file, line);
#endif
        return (char *)ptr;
    }
    return NULL;
}

void debug_free(void *user_ptr, const char *file, int line) {
    if (!user_ptr) return;

    debug_check_ptr(user_ptr, file, line);

    void *ptr = (char *)user_ptr - HEAD_MAGIC_SIZE;

    MemTrack **current = &mem_track_head;
    while (*current) {
        if ((*current)->ptr == ptr) {
            MemTrack *to_free = *current;
            *current = (*current)->next;
            free(to_free);
#if MALLOC_TRACKING
            fprintf(stderr, COLOR_GREEN "free %p at %s:%d\n" COLOR_RESET, ptr, file, line);
#endif
            break;
        }
        current = &(*current)->next;
    }
    free(ptr);
}

void debug_memory_leaks(const char *file, int line) {
    MemTrack *current = mem_track_head;
    while (current) {
        debug_check_ptr(current->ptr + HEAD_MAGIC_SIZE, file, line);

        fprintf(stderr, "Memory leak detected: %p allocated at %s:%d\n", current->ptr, current->file, current->line);
        current = current->next;
    }
}
