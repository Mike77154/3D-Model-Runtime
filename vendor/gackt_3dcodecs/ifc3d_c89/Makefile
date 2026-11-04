CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -O2 -Iinclude
AR ?= ar
ARFLAGS ?= rcs

SRC = src/ifc3d_util.c src/ifc3d_fixed.c src/ifc3d_type.c src/ifc3d_args.c src/ifc3d_step.c src/ifc3d_ifc.c src/ifc3d_mesh.c src/ifc3d_obj_writer.c
OBJ = $(SRC:.c=.o)

all: libifc3d.a ifc3d_demo

libifc3d.a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

ifc3d_demo: libifc3d.a examples/ifc3d_demo.c
	$(CC) $(CFLAGS) -o $@ examples/ifc3d_demo.c libifc3d.a

clean:
	rm -f $(OBJ) libifc3d.a ifc3d_demo samples/*.obj

check: all
	./ifc3d_demo samples/simple_extruded_wall.ifc samples/simple_extruded_wall.obj
	./ifc3d_demo samples/simple_tessellated_cube.ifc samples/simple_tessellated_cube.obj

.PHONY: all clean check
