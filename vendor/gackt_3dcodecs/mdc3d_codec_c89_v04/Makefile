CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude

all: mdc3d_dump mdc3d_obj mdc3d_lerp_obj mdc3d_skin_dump mdc3d_audit

mdc3d_dump: src/mdc3d.c examples/mdc3d_dump.c include/mdc3d.h
	$(CC) $(CFLAGS) src/mdc3d.c examples/mdc3d_dump.c -o $@

mdc3d_obj: src/mdc3d.c examples/mdc3d_obj.c include/mdc3d.h
	$(CC) $(CFLAGS) src/mdc3d.c examples/mdc3d_obj.c -o $@

mdc3d_lerp_obj: src/mdc3d.c examples/mdc3d_lerp_obj.c include/mdc3d.h
	$(CC) $(CFLAGS) src/mdc3d.c examples/mdc3d_lerp_obj.c -o $@

mdc3d_skin_dump: src/mdc3d.c examples/mdc3d_skin_dump.c include/mdc3d.h
	$(CC) $(CFLAGS) src/mdc3d.c examples/mdc3d_skin_dump.c -o $@

mdc3d_audit: src/mdc3d.c examples/mdc3d_audit.c include/mdc3d.h
	$(CC) $(CFLAGS) src/mdc3d.c examples/mdc3d_audit.c -o $@

check: all
	./mdc3d_dump tests/tiny.mdc >/dev/null
	./mdc3d_dump tests/tiny_compressed.mdc >/dev/null
	./mdc3d_dump tests/tiny_std112.mdc >/dev/null
	./mdc3d_obj tests/tiny_compressed.mdc 0 1 >/dev/null
	./mdc3d_lerp_obj tests/tiny_compressed.mdc 0 0 1 32768 >/dev/null
	./mdc3d_skin_dump tests/sample.skin >/dev/null
	./mdc3d_audit tests/tiny_compressed.mdc >/dev/null

clean:
	rm -f mdc3d_dump mdc3d_obj mdc3d_lerp_obj mdc3d_skin_dump mdc3d_audit
