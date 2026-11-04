CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude -Imini_codecs
OBJ = src/gltf3d.o src/gltf3d_validate.o src/gltf3d_ext.o src/gltf3d_util.o src/gltf3d_anim_eval.o src/gltf3d_decode.o
MINIOBJ = mini_codecs/gltf3d_mini_common.o mini_codecs/gltf3d_mini_images.o mini_codecs/gltf3d_mini_pngdec.o mini_codecs/gltf3d_mini_jpegparse.o mini_codecs/gltf3d_mini_jpegdec.o mini_codecs/gltf3d_mini_ktx2_basisu.o mini_codecs/gltf3d_mini_draco.o mini_codecs/gltf3d_mini_meshopt.o mini_codecs/gltf3d_mini_skinning.o mini_codecs/gltf3d_mini_texture_decode.o
EXE = gltf3d_dump gltf3d_inspect gltf3d_decode_demo gltf3d_mini_probe gltf3d_png_decode_demo gltf3d_jpeg_decode_demo gltf3d_texture_decode_demo

all: libgltf3d.a libgltf3d_mini.a $(EXE)

libgltf3d.a: $(OBJ)
	ar rcs libgltf3d.a $(OBJ)

libgltf3d_mini.a: $(MINIOBJ)
	ar rcs libgltf3d_mini.a $(MINIOBJ)

gltf3d_dump: examples/gltf3d_dump.o libgltf3d.a
	$(CC) $(CFLAGS) -o gltf3d_dump examples/gltf3d_dump.o libgltf3d.a

gltf3d_inspect: examples/gltf3d_inspect.o libgltf3d.a libgltf3d_mini.a
	$(CC) $(CFLAGS) -o gltf3d_inspect examples/gltf3d_inspect.o libgltf3d_mini.a libgltf3d.a

gltf3d_decode_demo: examples/gltf3d_decode_demo.o libgltf3d.a
	$(CC) $(CFLAGS) -o gltf3d_decode_demo examples/gltf3d_decode_demo.o libgltf3d.a

gltf3d_mini_probe: examples_mini/gltf3d_mini_probe.o libgltf3d_mini.a libgltf3d.a
	$(CC) $(CFLAGS) -o gltf3d_mini_probe examples_mini/gltf3d_mini_probe.o libgltf3d_mini.a libgltf3d.a

gltf3d_png_decode_demo: examples_mini/gltf3d_png_decode_demo.o libgltf3d_mini.a libgltf3d.a
	$(CC) $(CFLAGS) -o gltf3d_png_decode_demo examples_mini/gltf3d_png_decode_demo.o libgltf3d_mini.a libgltf3d.a

gltf3d_jpeg_decode_demo: examples_mini/gltf3d_jpeg_decode_demo.o libgltf3d_mini.a libgltf3d.a
	$(CC) $(CFLAGS) -o gltf3d_jpeg_decode_demo examples_mini/gltf3d_jpeg_decode_demo.o libgltf3d_mini.a libgltf3d.a

gltf3d_texture_decode_demo: examples_mini/gltf3d_texture_decode_demo.o libgltf3d_mini.a libgltf3d.a
	$(CC) $(CFLAGS) -o gltf3d_texture_decode_demo examples_mini/gltf3d_texture_decode_demo.o libgltf3d_mini.a libgltf3d.a

clean:
	rm -f $(OBJ) $(MINIOBJ) examples/gltf3d_dump.o examples/gltf3d_inspect.o examples/gltf3d_decode_demo.o examples_mini/gltf3d_mini_probe.o examples_mini/gltf3d_png_decode_demo.o examples_mini/gltf3d_jpeg_decode_demo.o examples_mini/gltf3d_texture_decode_demo.o libgltf3d.a libgltf3d_mini.a $(EXE)
