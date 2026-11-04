CC ?= cc
CFLAGS ?= -ansi -pedantic -Wall -Wextra -O2

all: hmb2obj make_sample_hmb make_sample_hmp

hmb2obj: tools/hmb2obj.c src/hmb3d.c src/hmb3d.h
	$(CC) $(CFLAGS) -o hmb2obj tools/hmb2obj.c src/hmb3d.c

make_sample_hmb: tests/make_sample_hmb.c
	$(CC) $(CFLAGS) -o make_sample_hmb tests/make_sample_hmb.c

make_sample_hmp: tests/make_sample_hmp.c
	$(CC) $(CFLAGS) -o make_sample_hmp tests/make_sample_hmp.c

sample: all
	./make_sample_hmb
	./hmb2obj sample_caligari_polh.hmb sample_caligari_polh.obj
	./make_sample_hmp
	./hmb2obj sample_assimp_hmp5.hmp sample_assimp_hmp5.obj

clean:
	rm -f hmb2obj make_sample_hmb make_sample_hmp sample_caligari_polh.hmb sample_caligari_polh.obj sample_assimp_hmp5.hmp sample_assimp_hmp5.obj
