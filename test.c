#include <signal.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ALLOCS 1024
#define MAX_ALLOC_SIZE 65536
#define MAX_ITERATIONS 1000000
#define REALLOC_PERCENTAGE 5
#define MAGIC_BYTE 0xc9

#ifdef __alignof_is_defined
#define EXPECTED_ALIGNMENT _Alignof(max_align_t)
#else
#define EXPECTED_ALIGNMENT 1
#endif

int running = 1;

void signal_handler(int sig)
{
    running = 0;
}

size_t check_region(unsigned char *region, size_t size)
{
    size_t overwritten_bytes = 0;
    for (size_t i = 0; i < size; i++) {
        if (region[i] != MAGIC_BYTE) {
            overwritten_bytes++;
        }
    }

    return overwritten_bytes;
}

int main()
{
    unsigned char *array[MAX_ALLOCS] = { 0 };
    int array_sizes[MAX_ALLOCS];
    int curr_allocated = 0;
    size_t total_allocations = 0;
    size_t total_reallocations = 0;
    size_t total_frees = 0;
    size_t total_overwritten_bytes = 0;
    size_t total_overwritten_regions = 0;
    size_t reallocation_errors = 0;
    size_t alignment_errors = 0;
    size_t iterations = 0;

    printf("memory allocation tester\n");

    srand(time(NULL));
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    clock_t tic = clock();

    while (running && iterations < MAX_ITERATIONS) {
        int r = rand() % MAX_ALLOCS;

        if (rand() % 100 < REALLOC_PERCENTAGE && curr_allocated > 0) {
            int i;

            do {
                i = rand() % MAX_ALLOCS;
            } while (array[i] == NULL);

            unsigned char *element = array[i];

            size_t overwritten_bytes = check_region(element, array_sizes[i]);

            total_overwritten_bytes += overwritten_bytes;

            if (overwritten_bytes > 0) {
                printf("error (iteration %zu): overwritten region found at %p, %zu bytes overwritten\n", iterations, element, overwritten_bytes);
                total_overwritten_regions++;
            }

            size_t new_size = rand() % MAX_ALLOC_SIZE;
            unsigned char *new_element = realloc(element, new_size);

            if ((uintptr_t) new_element % EXPECTED_ALIGNMENT != 0) {
                printf("realloc error: expected alignment %lu, got %d\n", EXPECTED_ALIGNMENT, (1 << (ffsll((uintptr_t) new_element) - 1)));
                alignment_errors++;
            }

            overwritten_bytes = check_region(new_element, array_sizes[i] > new_size ? new_size : array_sizes[i]);

            if (overwritten_bytes > 0) {
                printf("realloc error: miscopied region found at %p\n", element);
                reallocation_errors++;
            }

            memset(new_element, MAGIC_BYTE, new_size);

            array[i] = new_element;
            array_sizes[i] = new_size;

            total_reallocations++;
        } else {
            // this randomness tries to keep the amount of currently allocated regions at around MAX_ALLOCS / 2
            if (r >= curr_allocated) {
                for (int i = 0; i < MAX_ALLOCS; i++) {
                    if (array[i] == NULL) {
                        int size = rand() % MAX_ALLOC_SIZE;
                        array[i] = malloc(size);
                        array_sizes[i] = size;

                        if ((uintptr_t) array[i] % EXPECTED_ALIGNMENT != 0) {
                            printf("malloc error: expected alignment %lu, got %d\n", EXPECTED_ALIGNMENT, (1 << (ffsll((uintptr_t) array[i]) - 1)));
                            alignment_errors++;
                        }

                        // fill the allocated area with MAGIC_BYTE
                        memset(array[i], MAGIC_BYTE, size);

                        curr_allocated++;
                        total_allocations++;
                        break;
                    }
                }
            } else {
                int i;

                do {
                    i = rand() % MAX_ALLOCS;
                } while (array[i] == NULL);

                unsigned char *element = array[i];

                size_t overwritten_bytes = check_region(element, array_sizes[i]);

                total_overwritten_bytes += overwritten_bytes;

                if (overwritten_bytes > 0) {
                    printf("error (iteration %zu): overwritten region found at %p, %zu bytes overwritten\n", iterations, element, overwritten_bytes);
                    total_overwritten_regions++;
                }

                memset(element, 0, array_sizes[i]);

                array[i] = NULL;
                free(element);

                curr_allocated--;
                total_frees++;
            }
        }

        iterations++;
    }

    clock_t toc = clock();

    for(int i = 0; i < MAX_ALLOCS; i++) {
        if (array[i] != NULL) {
            unsigned char *element = array[i];

            int overwritten = 0;
            for (int j = 0; j < array_sizes[i]; j++) {
                if (element[j] != MAGIC_BYTE) {
                    overwritten = 1;
                    total_overwritten_bytes++;
                }
            }

            if (overwritten) {
                printf("error: overwritten region found at %p\n", element);
                total_overwritten_regions++;
            }

            free(element);
        }
    }

    printf("\nstatistics:\n");
    printf("iterations: %zu\n", iterations);
    printf("time spent: %.3f s\n", (double) (toc - tic) / CLOCKS_PER_SEC);
    printf("allocated at the time of stopping: %d\n", curr_allocated);
    printf("total allocations: %zu\n", total_allocations);
    printf("total reallocations: %zu\n", total_reallocations);
    printf("total frees: %zu\n", total_frees);
    printf("total bytes overwritten: %zu\n", total_overwritten_bytes);
    printf("total regions overwritten: %zu\n", total_overwritten_regions);
    printf("total reallocation errors: %zu\n", reallocation_errors);
    printf("total alignment errors: %zu\n", alignment_errors);
}
