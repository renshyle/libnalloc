#ifndef LIBNALLOC_H
#define LIBNALLOC_H

#include <stddef.h>

#ifndef LIBNALLOC_PAGE_SIZE
#define LIBNALLOC_PAGE_SIZE 4096
#endif

void *malloc(size_t size);
void *calloc(size_t nelem, size_t elsize);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

int posix_memalign(void **memptr, size_t alignment, size_t size);
void *aligned_alloc(size_t alignment, size_t size);

#endif
