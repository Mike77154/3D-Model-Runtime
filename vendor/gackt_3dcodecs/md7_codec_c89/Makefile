# md7_codec_c89 Makefile
CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Werror -Iinclude

all: md7dump md7_make_minimal md7toobj

md7dump: src/md7_codec.c demo/md7dump.c include/md7_codec.h
	$(CC) $(CFLAGS) src/md7_codec.c demo/md7dump.c -o md7dump

md7toobj: src/md7_codec.c demo/md7toobj.c include/md7_codec.h
	$(CC) $(CFLAGS) src/md7_codec.c demo/md7toobj.c -o md7toobj

md7_make_minimal: demo/md7_make_minimal.c
	$(CC) $(CFLAGS) demo/md7_make_minimal.c -o md7_make_minimal

test: all
	./md7_make_minimal tests/minimal_mdl7.mdl
	./md7dump tests/minimal_mdl7.mdl
	./md7toobj tests/minimal_mdl7.mdl tests/minimal_mdl7.obj

clean:
	rm -f md7dump md7_make_minimal md7toobj tests/minimal_mdl7.mdl tests/minimal_mdl7.obj
