# MSYS2 MinGW32: make -f Makefile CC=gcc
CC = gcc
CFLAGS = -O2 -std=c89 -pedantic -Wall -Wextra -Werror
COMMON = character_draw_bone_part.c hod2_binary.c hod2_corpus.c hod2_skin_audit.c hod2_skin_glb.c hod2_tables.c hod2_tablepack.c hod2_tablecsv.c hod2_pol.c hod2_tex.c hod2_mot.c hod2_glb.c \
         ../inherited/c89/hod2_resource_guard.c ../inherited/c89/hod2_extra_parts.c \
         ../inherited/c89/hod2_character_clips.c

all: hod2c89

hod2c89: $(COMMON) hod2_cli.c hod2_native.h
	$(CC) $(CFLAGS) $(COMMON) hod2_cli.c -o hod2c89

check: test_native test_character
	./test_native
	./test_character

test_native: $(COMMON) tests/test_native.c hod2_native.h
	$(CC) $(CFLAGS) $(COMMON) tests/test_native.c -o test_native

clean:
	rm -f hod2c89 hod2c89.exe test_native test_native.exe test_character test_character.exe

test_character: $(COMMON) tests/test_character.c hod2_native.h
	$(CC) $(CFLAGS) $(COMMON) tests/test_character.c -o test_character
