#include <pthread.h>
#include <stddef.h>
#include <sys/mman.h>

#include "libnalloc.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void libnalloc_lock(void)
{
    pthread_mutex_lock(&mutex);
}

void libnalloc_unlock(void)
{
    pthread_mutex_unlock(&mutex);
}

void *libnalloc_alloc(size_t pages)
{
    void *address = mmap(0, pages * LIBNALLOC_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (address == MAP_FAILED) {
        return NULL;
    }

    return address;
}

void libnalloc_free(void *ptr, size_t pages)
{
    munmap(ptr, pages * LIBNALLOC_PAGE_SIZE);
}
