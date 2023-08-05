CC = clang
OUT = libnalloc.so
TEST_OUT = test

.PHONY: all clean

$(OUT): libnalloc.c libnalloc.h
	$(CC) -Wall -pthread -O3 -shared -fPIC libnalloc.c -o $(OUT)

$(TEST_OUT): test.c
	$(CC) -O3 test.c -o $(TEST_OUT)

clean:
	rm -f $(OUT) $(TEST_OUT)
