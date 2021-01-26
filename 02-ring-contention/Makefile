CC:= gcc
CFLAGS:= -O3 -D_POSIX_SOURCE -D_GNU_SOURCE -m64
LIBS:= -lpthread -lrt

all: obj bin out plot sender sender-no-loads receiver

sender: obj/sender.o ../util/util.o ../util/util-cpu-specific.o
	$(CC) -o bin/$@ $^ $(LIBS)

sender-no-loads: obj/sender-no-loads.o ../util/util.o ../util/util-cpu-specific.o
	$(CC) -o bin/$@ $^ $(LIBS)

receiver: obj/receiver.o ../util/util.o ../util/util-cpu-specific.o
	$(CC) -o bin/$@ $^ $(LIBS)

obj/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

obj:
	mkdir -p $@

bin:
	mkdir -p $@

out:
	mkdir -p $@

plot:
	mkdir -p $@

clean:
	rm -rf bin obj

.PHONY: all clean
