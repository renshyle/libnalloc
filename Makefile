CC = clang
OUT = libnalloc.so
TEST_OUT = test

.PHONY: all clean

$(OUT): libnalloc.c linux.c include/libnalloc.h
	$(CC) -Wall -pthread -O3 -shared -fPIC -I include/ libnalloc.c linux.c -o $(OUT)

$(TEST_OUT): test.c
	$(CC) -O3 test.c -o $(TEST_OUT)

clean:
	rm -f $(OUT) $(TEST_OUT)
