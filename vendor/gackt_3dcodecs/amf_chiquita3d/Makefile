CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -DAMF_STRICT_C89_NO_LONGLONG -Iinclude
SRC = src/amf_core.c src/amf_reader.c src/amf_writer.c src/amf_zip.c src/amf_geom.c src/amf_topology.c src/amf_flatten.c src/amf_formula.c src/amf_export.c src/amf_zipwrite.c
OBJ = $(SRC:.c=.o)

all: libchiqui_amf.a test_amf amf_dump amf_convert

libchiqui_amf.a: $(OBJ)
	ar rcs $@ $(OBJ)

test_amf: tests/test_amf.c libchiqui_amf.a
	$(CC) $(CFLAGS) -o $@ tests/test_amf.c libchiqui_amf.a

amf_dump: tools/amf_dump.c libchiqui_amf.a
	$(CC) $(CFLAGS) -o $@ tools/amf_dump.c libchiqui_amf.a

amf_convert: tools/amf_convert.c libchiqui_amf.a
	$(CC) $(CFLAGS) -o $@ tools/amf_convert.c libchiqui_amf.a

clean:
	rm -f $(OBJ) libchiqui_amf.a test_amf amf_dump amf_convert
