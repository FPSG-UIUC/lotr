CC:= gcc
CFLAGS:= -O3 -D_POSIX_SOURCE -D_GNU_SOURCE -m64
LIBS:= -lpthread -lrt

all: obj bin out measure-slice-latency measure-slice-latency-optimized

measure-slice-latency: obj/measure-slice-latency.o ../util/util.o ../util/util-cpu-specific.o
	$(CC) -o bin/$@ $^ $(LIBS)

measure-slice-latency-optimized: obj/measure-slice-latency-optimized.o ../util/util.o ../util/util-cpu-specific.o
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
