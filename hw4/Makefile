CC=gcc
CFLAGS=-g -ggdb3 -Wall -std=gnu99
LDFLAGS=-pthread
EXECUTABLES=basicserver forkserver threadserver poolserver
SOURCE=server.c
HELPERS=wq.c server_config.c http_helper.c server_signal.c
OBJECTS=$(HELPERS:.c=.o)

all: $(EXECUTABLES)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

basicserver: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -D BASICSERVER $(SOURCE) $(OBJECTS) -o $@
forkserver: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -D FORKSERVER $(SOURCE) $(OBJECTS) -o $@
threadserver: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -D THREADSERVER $(SOURCE) $(OBJECTS) -o $@
poolserver: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -D POOLSERVER $(SOURCE) $(OBJECTS) -o $@

clean:
	rm -f $(EXECUTABLES) ${OBJECTS}
