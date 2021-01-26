CC:= gcc
CFLAGS:= -O3 -D_POSIX_SOURCE -D_GNU_SOURCE -m64
LIBS:= -lpthread -lrt

all: allocate-and-print

allocate-and-print: allocate-and-print.o ../util.o ../util-cpu-specific.o
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -rf *.o allocate-and-print

.PHONY: all clean
