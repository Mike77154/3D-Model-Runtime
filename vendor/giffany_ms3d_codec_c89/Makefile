CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB = libgif_ms3d_codec.a
OBJ = src/gif_ms3d_codec.o src/gif_ms3d_normals.o src/gif_ms3d_skin.o src/gif_ms3d_quat.o src/gif_ms3d_uv.o src/gif_ms3d_mtl.o src/gif_ms3d_player.o src/gif_ms3d_render.o

all: $(LIB) examples/ms3d_info examples/ms3d_spec_audit examples/make_tiny_ms3d examples/ms3d_obj_export examples/ms3d_probe examples/ms3d_anim_obj tests/test_ms3d_skin tests/test_ms3d_codec tests/test_ms3d_extra tests/test_ms3d_normals tests/test_ms3d_uv tests/test_ms3d_player tests/test_ms3d_quat examples/ms3d_player_probe examples/ms3d_player_stream_obj examples/ms3d_v17_corpus_probe tests/test_ms3d_v17

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $(OBJ)

src/gif_ms3d_quat.o: src/gif_ms3d_quat.c include/gif_ms3d_quat.h
	$(CC) $(CFLAGS) -c src/gif_ms3d_quat.c -o src/gif_ms3d_quat.o

src/gif_ms3d_skin.o: src/gif_ms3d_skin.c include/gif_ms3d_skin.h include/gif_ms3d_quat.h include/gif_ms3d_normals.h
	$(CC) $(CFLAGS) -c src/gif_ms3d_skin.c -o src/gif_ms3d_skin.o

src/gif_ms3d_normals.o: src/gif_ms3d_normals.c include/gif_ms3d_normals.h include/gif_ms3d_codec.h
	$(CC) $(CFLAGS) -c src/gif_ms3d_normals.c -o src/gif_ms3d_normals.o

src/gif_ms3d_uv.o: src/gif_ms3d_uv.c include/gif_ms3d_uv.h include/gif_ms3d_codec.h
	$(CC) $(CFLAGS) -c src/gif_ms3d_uv.c -o src/gif_ms3d_uv.o

src/gif_ms3d_mtl.o: src/gif_ms3d_mtl.c include/gif_ms3d_mtl.h include/gif_ms3d_codec.h
	$(CC) $(CFLAGS) -c src/gif_ms3d_mtl.c -o src/gif_ms3d_mtl.o

src/gif_ms3d_player.o: src/gif_ms3d_player.c include/gif_ms3d_player.h include/gif_ms3d_uv.h
	$(CC) $(CFLAGS) -c src/gif_ms3d_player.c -o src/gif_ms3d_player.o

src/gif_ms3d_render.o: src/gif_ms3d_render.c include/gif_ms3d_render.h
	$(CC) $(CFLAGS) -c src/gif_ms3d_render.c -o src/gif_ms3d_render.o

src/gif_ms3d_codec.o: src/gif_ms3d_codec.c include/gif_ms3d_codec.h
	$(CC) $(CFLAGS) -c src/gif_ms3d_codec.c -o src/gif_ms3d_codec.o

examples/ms3d_spec_audit: examples/ms3d_spec_audit.c $(LIB)
	$(CC) $(CFLAGS) examples/ms3d_spec_audit.c $(LIB) -o examples/ms3d_spec_audit

examples/ms3d_info: examples/ms3d_info.c $(LIB)
	$(CC) $(CFLAGS) examples/ms3d_info.c $(LIB) -o examples/ms3d_info

examples/make_tiny_ms3d: examples/make_tiny_ms3d.c
	$(CC) $(CFLAGS) examples/make_tiny_ms3d.c -o examples/make_tiny_ms3d

examples/ms3d_obj_export: examples/ms3d_obj_export.c $(LIB)
	$(CC) $(CFLAGS) examples/ms3d_obj_export.c $(LIB) -o examples/ms3d_obj_export

examples/ms3d_probe: examples/ms3d_probe.c $(LIB)
	$(CC) $(CFLAGS) examples/ms3d_probe.c $(LIB) -o examples/ms3d_probe

examples/ms3d_anim_obj: examples/ms3d_anim_obj.c $(LIB)
	$(CC) $(CFLAGS) examples/ms3d_anim_obj.c $(LIB) -o examples/ms3d_anim_obj

tests/test_ms3d_uv: tests/test_ms3d_uv.c $(LIB)
	$(CC) $(CFLAGS) tests/test_ms3d_uv.c $(LIB) -o tests/test_ms3d_uv

tests/test_ms3d_quat: tests/test_ms3d_quat.c $(LIB)
	$(CC) $(CFLAGS) tests/test_ms3d_quat.c $(LIB) -o tests/test_ms3d_quat

tests/test_ms3d_skin: tests/test_ms3d_skin.c $(LIB)
	$(CC) $(CFLAGS) tests/test_ms3d_skin.c $(LIB) -o tests/test_ms3d_skin

tests/test_ms3d_codec: tests/test_ms3d_codec.c $(LIB)
	$(CC) $(CFLAGS) tests/test_ms3d_codec.c $(LIB) -o tests/test_ms3d_codec

tests/test_ms3d_extra: tests/test_ms3d_extra.c $(LIB)
	$(CC) $(CFLAGS) tests/test_ms3d_extra.c $(LIB) -o tests/test_ms3d_extra

tests/test_ms3d_normals: tests/test_ms3d_normals.c $(LIB)
	$(CC) $(CFLAGS) tests/test_ms3d_normals.c $(LIB) -o tests/test_ms3d_normals

tests/test_ms3d_player: tests/test_ms3d_player.c $(LIB)
	$(CC) $(CFLAGS) tests/test_ms3d_player.c $(LIB) -o tests/test_ms3d_player

examples/ms3d_player_probe: examples/ms3d_player_probe.c $(LIB)
	$(CC) $(CFLAGS) examples/ms3d_player_probe.c $(LIB) -o examples/ms3d_player_probe

examples/ms3d_player_stream_obj: examples/ms3d_player_stream_obj.c $(LIB)
	$(CC) $(CFLAGS) examples/ms3d_player_stream_obj.c $(LIB) -o examples/ms3d_player_stream_obj

tests/test_ms3d_v17: tests/test_ms3d_v17.c $(LIB)
	$(CC) $(CFLAGS) tests/test_ms3d_v17.c $(LIB) -o tests/test_ms3d_v17

check: all
	./tests/test_ms3d_codec
	./tests/test_ms3d_extra
	./tests/test_ms3d_normals
	./tests/test_ms3d_skin
	./tests/test_ms3d_uv
	./tests/test_ms3d_player
	./tests/test_ms3d_quat
	./tests/test_ms3d_v17
	./examples/make_tiny_ms3d tests/tiny.ms3d
	./examples/ms3d_info tests/tiny.ms3d

clean:
	rm -f $(OBJ) $(LIB) examples/ms3d_info examples/ms3d_spec_audit examples/ms3d_spec_audit examples/make_tiny_ms3d examples/ms3d_obj_export examples/ms3d_probe examples/ms3d_anim_obj tests/test_ms3d_skin tests/test_ms3d_codec tests/test_ms3d_extra tests/test_ms3d_normals tests/test_ms3d_uv tests/test_ms3d_skin examples/ms3d_anim_obj tests/tiny.ms3d tests/test_ms3d_player tests/test_ms3d_quat examples/ms3d_player_probe tests/test_ms3d_v17 examples/ms3d_v17_corpus_probe

examples/ms3d_v17_corpus_probe: examples/ms3d_v17_corpus_probe.c $(LIB)
	$(CC) $(CFLAGS) examples/ms3d_v17_corpus_probe.c $(LIB) -o examples/ms3d_v17_corpus_probe
