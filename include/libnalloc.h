#ifndef LIBNALLOC_H
#define LIBNALLOC_H

#include <stddef.h>

#ifndef LIBNALLOC_PAGE_SIZE
#define LIBNALLOC_PAGE_SIZE 4096
#endif

#define PASTER(x, y) x ## y
#define EVALUATOR(x, y) PASTER(x, y)
#define LIBNALLOC_EXPORT(name) EVALUATOR(LIBNALLOC_PREFIX, name)

void *LIBNALLOC_EXPORT(malloc)(size_t size);
void *LIBNALLOC_EXPORT(calloc)(size_t nelem, size_t elsize);
void *LIBNALLOC_EXPORT(realloc)(void *ptr, size_t size);
void LIBNALLOC_EXPORT(free)(void *ptr);

int LIBNALLOC_EXPORT(posix_memalign)(void **memptr, size_t alignment, size_t size);
void *LIBNALLOC_EXPORT(aligned_alloc)(size_t alignment, size_t size);

#endif
