CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB = libprj_c89.a
OBJ = src/prj_c89.o

all: $(LIB) prj_demo test_prj

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $(OBJ)

src/prj_c89.o: src/prj_c89.c include/prj_c89.h
	$(CC) $(CFLAGS) -c src/prj_c89.c -o src/prj_c89.o

prj_demo: examples/demo.c $(LIB)
	$(CC) $(CFLAGS) examples/demo.c $(LIB) -o prj_demo

test_prj: tests/test_prj.c $(LIB)
	$(CC) $(CFLAGS) tests/test_prj.c $(LIB) -o test_prj

check: test_prj prj_demo
	./test_prj
	./prj_demo samples/web_mercator.prj >/dev/null
	./prj_demo samples/wkt2_wgs84.prj >/dev/null
	./prj_demo samples/old_arcinfo.prj >/dev/null

clean:
	rm -f $(OBJ) $(LIB) prj_demo test_prj

.PHONY: all check clean
