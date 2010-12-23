CFLAGS=-Wall -g  -fno-strength-reduce


all: nuless

nuless:	nuless.c
	gcc $(CFLAGS) nuless.c -lncurses -o nuless
# 	strip nuless
	du -h nuless

install:
	cp nuless /usr/local/bin
clean:
	rm -f nuless