CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2
AR ?= ar
ARFLAGS ?= rcs

all: libg3d_codec.a selftest g3dtool mk_sample

g3d_codec.o: g3d_codec.c g3d_codec.h g3d_kib.h
	$(CC) $(CFLAGS) -c g3d_codec.c -o g3d_codec.o

g3d_kib.o: g3d_kib.c g3d_kib.h g3d_codec.h
	$(CC) $(CFLAGS) -c g3d_kib.c -o g3d_kib.o

libg3d_codec.a: g3d_codec.o g3d_kib.o
	$(AR) $(ARFLAGS) $@ g3d_codec.o g3d_kib.o

selftest: selftest.c libg3d_codec.a g3d_codec.h g3d_kib.h
	$(CC) $(CFLAGS) selftest.c libg3d_codec.a -o selftest

g3dtool: g3d_tool.c libg3d_codec.a g3d_codec.h g3d_kib.h
	$(CC) $(CFLAGS) g3d_tool.c libg3d_codec.a -o g3dtool

mk_sample: mk_sample.c libg3d_codec.a g3d_codec.h g3d_kib.h
	$(CC) $(CFLAGS) mk_sample.c libg3d_codec.a -o mk_sample

test: selftest g3dtool
	./selftest

smoke: all
	./smoke.sh

clean:
	rm -f g3d_codec.o g3d_kib.o libg3d_codec.a selftest g3dtool mk_sample
	rm -rf _smoke
