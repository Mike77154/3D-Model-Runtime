CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Werror -O2
AR ?= ar
ARFLAGS ?= rcs

INCLUDES = -Iinclude
SRC = \
	src/xps_api.c \
	src/xps_detect.c \
	src/xps_util.c \
	src/xps_fx.c \
	src/xps_io.c \
	src/xps_header.c \
	src/xps_ascii.c \
	src/xps_bin.c \
	src/xps_pose.c \
	src/xps_bonedict.c \
	src/xps_scene.c
OBJ = $(SRC:.c=.o)

all: libxpsimport_c89.a example_dump example_obj example_obj_mtl

libxpsimport_c89.a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

example_dump: examples/example_dump.o libxpsimport_c89.a
	$(CC) $(CFLAGS) -o $@ examples/example_dump.o libxpsimport_c89.a

example_obj: examples/example_obj.o libxpsimport_c89.a
	$(CC) $(CFLAGS) -o $@ examples/example_obj.o libxpsimport_c89.a

example_obj_mtl: examples/example_obj_mtl.o libxpsimport_c89.a
	$(CC) $(CFLAGS) -o $@ examples/example_obj_mtl.o libxpsimport_c89.a

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(OBJ) examples/example_dump.o examples/example_obj.o examples/example_obj_mtl.o libxpsimport_c89.a example_dump example_obj example_obj_mtl

.PHONY: all clean
