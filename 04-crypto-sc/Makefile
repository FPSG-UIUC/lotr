CC:= gcc
CFLAGS:= -O3 -D_POSIX_SOURCE -D_GNU_SOURCE -m64
LIBS:= -lpthread -lrt

all: obj bin out ringbus-monitor

ringbus-monitor: obj/ringbus-monitor.o ../util/util.o ../util/util-cpu-specific.o
	$(CC) -o bin/$@ $^ $(LIBS)

obj/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

obj:
	mkdir -p $@

bin:
	mkdir -p $@

out:
	mkdir -p $@

clean:
	rm -rf bin obj

.PHONY: all clean
