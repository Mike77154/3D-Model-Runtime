CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Iinclude
AR ?= ar

LIB = libvrml97_codec.a
OBJ = src/vrml97_codec.o
TOOL = vrml97_to_obj

all: $(LIB) $(TOOL)

$(LIB): $(OBJ)
	$(AR) rcs $(LIB) $(OBJ)

src/vrml97_codec.o: src/vrml97_codec.c include/vrml97_codec.h
	$(CC) $(CFLAGS) -c src/vrml97_codec.c -o src/vrml97_codec.o

$(TOOL): tools/vrml97_to_obj.c $(LIB) include/vrml97_codec.h
	$(CC) $(CFLAGS) tools/vrml97_to_obj.c $(LIB) -o $(TOOL)

clean:
	rm -f $(OBJ) $(LIB) $(TOOL) out.obj out.mtl examples/*.obj examples/*.mtl
