#include <stddef.h>
#include <stdint.h>

#include <libnalloc.h>

#ifndef LIBNALLOC_PREALLOC_PAGES
// this many pages will be requested in addition to the required amount when requesting memory from the operating system
#define LIBNALLOC_PREALLOC_PAGES 16
#endif

#ifndef LIBNALLOC_DIRECT_THRESHOLD
// allocations above this size will be directly requested from the operating system
#define LIBNALLOC_DIRECT_THRESHOLD 32768
#endif

// these four functions need to be implemented for libnalloc to work

// allocates `pages` * `LIBNALLOC_PAGE_SIZE` amount of contiguous memory
// the address returned must be aligned to LIBNALLOC_ALIGNMENT
// returns NULL on error
void *libnalloc_alloc(size_t pages);

// frees `pages` * `LIBNALLOC_PAGE_SIZE` amount of memory starting at ptr
// `ptr` is a value previously returned by `libnalloc_alloc`
void libnalloc_free(void *ptr, size_t pages);

// locks the memory allocator
void libnalloc_lock(void);

// unlocks the memory allocator
void libnalloc_unlock(void);

#define LIBNALLOC_ALIGNMENT 16

#define LIBNALLOC_MIN_ALLOC sizeof(struct free_block_data)

void *memset(void *s, int c, size_t n);
void *memcpy(void *restrict s1, const void *restrict s2, size_t n);

struct block_header {
    uint64_t size : 62;
    uint64_t direct : 1;
    uint64_t free : 1;
};

struct direct_header {
    uintptr_t map_start;
};

struct free_block_data {
    struct free_block *next;
    struct free_block *prev;
};

struct free_block {
    struct block_header header;
    struct free_block_data data;
};

struct chunk_header {
    struct chunk_header *next;
    struct free_block *free_block;
    uint64_t size;
};

static void copy_block_header_to_end(struct block_header *block);
static void *chunk_alloc(struct chunk_header *chunk, size_t size);
static void *alloc_direct(size_t size, size_t alignment);

struct chunk_header *first_chunk = NULL;

static void copy_block_header_to_end(struct block_header *block)
{
    struct block_header *block_end = (struct block_header*) ((uintptr_t) block + block->size * LIBNALLOC_ALIGNMENT - sizeof(struct block_header));
    block_end->free = block->free;
    block_end->direct = block->direct;
    block_end->size = block->size;
}

static void *chunk_alloc(struct chunk_header *chunk, size_t size)
{
    struct free_block *block = chunk->free_block;
    while (block != NULL) {
        if (block->header.size * LIBNALLOC_ALIGNMENT >= size + 2 * sizeof(struct block_header)) {
            uint64_t orig_size = block->header.size;

            block->header.free = 0;
            block->header.size = (size + 2 * sizeof(struct block_header)) / LIBNALLOC_ALIGNMENT;

            if (block->data.prev == NULL) {
                chunk->free_block = block->data.next;
            } else {
                block->data.prev->data.next = block->data.next;
            }

            if (block->data.next != NULL) {
                block->data.next->data.prev = block->data.prev;
            }

            if ((orig_size - block->header.size) * LIBNALLOC_ALIGNMENT >= 2 * sizeof(struct block_header) + LIBNALLOC_MIN_ALLOC) {
                // create a new free block from the remaining space in the found block

                struct free_block *new_block = (struct free_block*) ((uintptr_t) block + block->header.size * LIBNALLOC_ALIGNMENT);
                new_block->header.free = 1;
                new_block->header.direct = 0;
                new_block->header.size = orig_size - block->header.size;

                new_block->data.prev = NULL;
                new_block->data.next = NULL;

                if (chunk->free_block != NULL) {
                    chunk->free_block->data.prev = new_block;
                    new_block->data.next = chunk->free_block;
                }

                chunk->free_block = new_block;

                copy_block_header_to_end(&new_block->header);
            } else {
                // expand the allocated block to the whole found block as there is not enough space to create a new free block
                block->header.size = orig_size;
            }

            copy_block_header_to_end(&block->header);

            return &block->data;
        }

        block = block->data.next;
    }

    return NULL;
}

static void *alloc_direct(size_t size, size_t alignment)
{
    uint64_t direct_offset = ((alignment + sizeof(struct direct_header) + sizeof(struct block_header) - 1) / alignment) * alignment - sizeof(struct direct_header) - sizeof(struct block_header);
    uint64_t direct_size = (direct_offset + sizeof(struct direct_header) + sizeof(struct block_header) + size + LIBNALLOC_PAGE_SIZE - 1) / LIBNALLOC_PAGE_SIZE;

    void *address = libnalloc_alloc(direct_size);

    if (address == NULL) {
        return NULL;
    }

    struct direct_header *direct = (struct direct_header*) ((uintptr_t) address + direct_offset);
    struct block_header *block = (struct block_header*) (direct + 1);

    block->free = 0;
    block->direct = 1;
    block->size = direct_size * LIBNALLOC_PAGE_SIZE / LIBNALLOC_ALIGNMENT;

    direct->map_start = (uintptr_t) address;

    return block + 1;
}

void *malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    if (size >= LIBNALLOC_DIRECT_THRESHOLD) {
        return alloc_direct(size, LIBNALLOC_ALIGNMENT);
    }

    // align size up to LIBNALLOC_ALIGNMENT
    size = ((size + LIBNALLOC_ALIGNMENT - 1) / LIBNALLOC_ALIGNMENT) * LIBNALLOC_ALIGNMENT;

    if (size < LIBNALLOC_MIN_ALLOC) {
        size = LIBNALLOC_MIN_ALLOC;
    }

    libnalloc_lock();

    struct chunk_header *chunk = first_chunk;
    while (chunk != NULL) {
        void *res = chunk_alloc(chunk, size);

        if (res != NULL) {
            libnalloc_unlock();
            return res;
        }

        chunk = chunk->next;
    }

    // currently allocated chunks don't have enough space, allocate new chunk with enough space

    size_t pages_to_alloc = (size + sizeof(struct chunk_header) + 5 * sizeof(struct block_header) + LIBNALLOC_PAGE_SIZE - 1) / LIBNALLOC_PAGE_SIZE + LIBNALLOC_PREALLOC_PAGES;
    chunk = libnalloc_alloc(pages_to_alloc);

    if (chunk == NULL) {
        libnalloc_unlock();
        return NULL;
    }

    struct block_header *block = (struct block_header*) (chunk + 1);
    block->free = 0;
    block->direct = 0;
    block->size = (size + 2 * sizeof(struct block_header)) / LIBNALLOC_ALIGNMENT;
    copy_block_header_to_end(block);

    struct free_block *free_block = (struct free_block*) ((uintptr_t) block + block->size * LIBNALLOC_ALIGNMENT);
    free_block->header.free = 1;
    free_block->header.direct = 0;
    free_block->header.size = (pages_to_alloc * LIBNALLOC_PAGE_SIZE - sizeof(struct chunk_header) - size - 3 * sizeof(struct block_header)) / LIBNALLOC_ALIGNMENT;
    copy_block_header_to_end(&free_block->header);
    free_block->data.next = NULL;
    free_block->data.prev = NULL;

    chunk->free_block = free_block;

    block = (struct block_header*) ((uintptr_t) free_block + free_block->header.size * LIBNALLOC_ALIGNMENT);
    block->size = 0;
    block->direct = 0;
    block->free = 0;

    chunk->size = pages_to_alloc * LIBNALLOC_PAGE_SIZE;
    chunk->next = first_chunk;
    first_chunk = chunk;

    libnalloc_unlock();

    return (void*) ((uintptr_t) chunk + sizeof(struct chunk_header) + sizeof(struct block_header));
}

void *calloc(size_t nelem, size_t elsize)
{
    void *tgt = malloc(nelem * elsize);

    if (tgt != NULL) {
        memset(tgt, 0, nelem * elsize);
    }

    return tgt;
}

void *realloc(void *ptr, size_t size)
{
    // some programs (namely grep) take issue with realloc(NULL, 0) returning a NULL pointer
    if (size == 0 && ptr == NULL) {
        size = LIBNALLOC_MIN_ALLOC;
    }

    void *tgt = malloc(size);

    if (tgt != NULL && ptr != NULL) {
        struct block_header *block = (struct block_header*) ((uintptr_t) ptr - sizeof(struct block_header));
        size_t old_data_size = block->size * LIBNALLOC_ALIGNMENT - 2 * sizeof(struct block_header);

        memcpy(tgt, ptr, size > old_data_size ? old_data_size : size);
        free(ptr);
    }

    return tgt;
}

void free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    struct free_block *block = (struct free_block*) ((uintptr_t) ptr - sizeof(struct block_header));

    if (block->header.direct) {
        struct direct_header *direct = (struct direct_header*) ((uintptr_t) block - sizeof(struct direct_header));

        libnalloc_free((void*) direct->map_start, block->header.size * LIBNALLOC_ALIGNMENT / LIBNALLOC_PAGE_SIZE);
        return;
    }

    libnalloc_lock();

    block->header.free = 1;
    block->data.next = NULL;
    block->data.prev = NULL;

    // find the chunk that owns this block
    struct chunk_header *chunk = first_chunk;
    struct chunk_header *prev_chunk = NULL;
    while (chunk != NULL) {
        if ((uintptr_t) chunk < (uintptr_t) block && (uintptr_t) chunk + chunk->size > (uintptr_t) block) {
            break; // found
        }

        prev_chunk = chunk;
        chunk = chunk->next;
    }

    // append block to the beginning of the chunk's linked list of free blocks

    if (chunk->free_block != NULL) {
        chunk->free_block->data.prev = block;
        block->data.next = chunk->free_block;
    }

    chunk->free_block = block;

    // merge with next block if it is free

    struct free_block *next_block = (struct free_block*) ((uintptr_t) block + block->header.size * LIBNALLOC_ALIGNMENT);
    if (next_block->header.size != 0 && next_block->header.free) {
        block->header.size += next_block->header.size;

        next_block->data.prev->data.next = next_block->data.next;

        if (next_block->data.next != NULL) {
            next_block->data.next->data.prev = next_block->data.prev;
        }
    }

    // merge with previous block if it is free

    if ((uintptr_t) block - sizeof(struct chunk_header) != (uintptr_t) chunk) {
        struct block_header *prev_block_header = &block->header - 1;

        if (prev_block_header->free) {
            struct free_block *prev_block = (struct free_block*) ((uintptr_t) block - prev_block_header->size * LIBNALLOC_ALIGNMENT);
            prev_block->header.size += block->header.size;

            block->data.next->data.prev = NULL;
            chunk->free_block = chunk->free_block->data.next;

            block = prev_block;
        }
    }

    // free the chunk containing this block if the chunk is fully free

    if ((uintptr_t) chunk == (uintptr_t) block - sizeof(struct chunk_header) && chunk->size == block->header.size * LIBNALLOC_ALIGNMENT + sizeof(struct chunk_header) + sizeof(struct block_header)) {
        // the chunk containing the block is fully free, don't free if only one chunk is allocated
        if (first_chunk != chunk || chunk->next != NULL) {
            if (prev_chunk != NULL) {
                prev_chunk->next = chunk->next;
            }

            if (first_chunk == chunk) {
                first_chunk = chunk->next;
            }

            libnalloc_free(chunk, chunk->size / LIBNALLOC_PAGE_SIZE);

            libnalloc_unlock();
            return;
        }
    }

    copy_block_header_to_end(&block->header);

    libnalloc_unlock();
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
    void *ptr = aligned_alloc(alignment, size);
    *memptr = ptr;

    return 0;
}

void *aligned_alloc(size_t alignment, size_t size)
{
    if (size == 0 || alignment == 0) {
        return NULL;
    }

    if (alignment <= LIBNALLOC_ALIGNMENT) {
        return malloc(size);
    }

    // easier to just do a direct allocation regardless of size
    return alloc_direct(size, alignment);
}
