CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -pedantic -std=c89 -Iinclude

LIB_OBJS = \
    src/md2_error.o \
    src/md2_fixed.o \
    src/md2_model.o \
    src/md2_validate.o \
    src/md2_io.o \
    src/md2_glcmd.o \
    src/md2_glcmd_build.o \
    src/md2_heuristic.o \
    src/md2_decode.o \
    src/md2_encode.o \
    src/md2_anim.o \
    src/md2_runtime.o \
    src/md2_gpu.o

all: libmd2fx.a md2_selftest

libmd2fx.a: $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

md2_selftest: examples/md2_selftest.o libmd2fx.a
	$(CC) $(CFLAGS) -o $@ examples/md2_selftest.o libmd2fx.a

md2_probe: examples/md2_probe.o libmd2fx.a
	$(CC) $(CFLAGS) -o $@ examples/md2_probe.o libmd2fx.a

md2_rebuild: examples/md2_rebuild.o libmd2fx.a
	$(CC) $(CFLAGS) -o $@ examples/md2_rebuild.o libmd2fx.a

probe: md2_probe

rebuild-tool: md2_rebuild

audit-noheap:
	sh tools/noheap_audit.sh

audit: audit-noheap

examples/md2_selftest.o: examples/md2_selftest.c
	$(CC) $(CFLAGS) -c -o $@ $<

examples/md2_probe.o: examples/md2_probe.c
	$(CC) $(CFLAGS) -c -o $@ $<

examples/md2_rebuild.o: examples/md2_rebuild.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o examples/*.o libmd2fx.a md2_selftest md2_probe md2_rebuild examples/demo.md2

.PHONY: all clean probe rebuild-tool audit audit-noheap
