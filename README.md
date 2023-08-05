# libnalloc
a simple, very easy to port memory allocator for 64-bit systems

## Usage

libnalloc can be built as a shared library for Linux using `make`. Programs can be made to use that shared library using `LD_PRELOAD`. The memory allocator can be configured using the configurable options near the top of `libnalloc.c`. They can be used to change the memory usage or performance of `libnalloc` depending on the workload.

## Testing

libnalloc comes with a test suite that can be built using `make test`. The built binary, `test`, tests malloc, realloc and free by calling them randomly for a set number of times and checks that no allocated values are overwritten by the memory allocator. The test suite has a few configurable options in `test.c`. By default the test suite uses the C library's malloc and free, in order to test libnalloc using `test`, you can use `LD_PRELOAD` to use libnalloc's malloc and free: `LD_PRELOAD=libnalloc.so ./test`.

## Porting

Porting libnalloc to another operating system is very easy as libnalloc requires only four functions to be implemented in order to work. `libnalloc.c` has a Linux implementation at the bottom which you should rewrite for your operating system, and the function prototypes near the top of the file contain essential information on the functions. You can then build a shared library for your operating system or link libnalloc statically to your operating system's programs.

## Supported functions

- [x] `malloc`
- [x] `realloc`
- [x] `calloc`
- [x] `free`
- [x] `posix_memalign`
- [x] `aligned_alloc`
- [ ] `memalign`
- [ ] `malloc_usable_size`
- [ ] `pvalloc`
- [ ] `valloc`

## TODO

- [ ] 32-bit support
- [ ] `errno`
