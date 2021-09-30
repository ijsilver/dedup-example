CC = g++
CFLAGS+=-Wall -W -ggdb3 -std=gnu99 -O2


.PHONY: all clean clean-rabin

all: rabin-cdc

main.o: main.c
#	g++ -ggdb3 -std=gnu99 -O2 -c -o $@ $^


rabin.o: rabin.c rabin.h
sha1.o: sha1.c sha1.h

rabin-cdc: main.o rabin.o sha1.o
	$(CC) $(CFLAGS) -o $@ $^ -pthread
#	g++ $(CFLAGS)  $^

clean: clean-rabin

clean-rabin:
	rm -f rabin.o main.o rabin-cdc
