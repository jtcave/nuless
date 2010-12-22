#version:=$(shell echo `cat version.h | awk '{print $$3}'`)
OPT=-O2  -fno-strength-reduce


all: nuless


#chver:
#	@echo "#define VERSION `expr $(version) + 1`" > version.h	

nuless:	nuless.c
	gcc $(OPT) nuless.c -lncurses -o nuless
	# strip nuless
	du -h nuless

install:
	cp nuless /usr/local/bin
clean:
	rm -f nuless


#tgz:	lsm
#	tar -cf- README *.lsm nuless nuless.c Makefile demo \
#	| gzip -c > muless-0.$(version).tgz

#lsm:
#	makelsm

#export:
#	export.sunsite
