CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra -pedantic -Iinclude

all: test_obj_import

test_obj_import: src/gobj_obj.c src/gobj_mtl.c examples/test_obj_import.c include/gobj_obj.h include/gobj_mtl.h include/gobj_fixed.h
	$(CC) $(CFLAGS) src/gobj_obj.c src/gobj_mtl.c examples/test_obj_import.c -o test_obj_import

run: test_obj_import
	./test_obj_import

check-static:
	sh tests/no_dynamic_alloc_grep.sh

clean:
	rm -f test_obj_import
