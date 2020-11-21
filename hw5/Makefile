CFLAGS=-g -Wall -std=c99 -D_POSIX_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -fPIC
TEST_CFLAGS=-Wl,-rpath=.
TEST_LDFLAGS=-ldl
OBJECTS=debug.o list.o mm_alloc.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

hw3lib.so: mm_alloc.o
	$(CC) -shared -o $@ $^

mm_test: mm_test.c
	$(CC) $(CFLAGS) $(TEST_CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

clean:
	rm -rf hw3lib.so mm_test *.o