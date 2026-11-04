CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Iinclude

all: example_parse example_stream

example_parse: src/vta_decoder3d.c examples/example_parse.c include/vta_decoder3d.h
	$(CC) $(CFLAGS) -o example_parse src/vta_decoder3d.c examples/example_parse.c

example_stream: src/vta_decoder3d.c examples/example_stream.c include/vta_decoder3d.h
	$(CC) $(CFLAGS) -o example_stream src/vta_decoder3d.c examples/example_stream.c

run: example_parse
	./example_parse samples/minimal.vta

stream: example_stream
	./example_stream samples/minimal.vta

clean:
	rm -f example_parse example_parse.exe example_stream example_stream.exe
