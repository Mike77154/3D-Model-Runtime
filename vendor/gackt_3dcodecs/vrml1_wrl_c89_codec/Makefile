CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
INCLUDES = -Iinclude
LIBSRC = src/vr1_fixed.c src/vr1_lexer.c src/vr1_scene.c src/vr1_parser.c src/vr1_export_obj.c src/vr1_export_cmesh.c
OBJ = $(LIBSRC:.c=.o)

all: vr1dump vr1cmesh

vr1dump: $(LIBSRC) tools/vr1dump.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(LIBSRC) tools/vr1dump.c

vr1cmesh: $(LIBSRC) tools/vr1cmesh.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(LIBSRC) tools/vr1cmesh.c

clean:
	rm -f vr1dump vr1cmesh smoke $(OBJ) *.obj *.o

.PHONY: all clean
