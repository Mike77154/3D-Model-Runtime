CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude

SRC = src/dts89_core.c src/dts89_probe.c src/dts89_decode.c src/dts89_encode.c src/dts89_detach.c src/dts89_tscript.c src/dts89_dsq.c src/dts89_runtime.c src/dts89_validate.c src/dts89_anim.c src/dts89_animer.c src/dts89_posefeamw.c src/dts89_uv_texturer.c src/dts89_mesher.c src/dts89_texturer_opener.c src/dts89_shaderer.c src/dts89_lighting40.c src/dts89_lod41.c
OBJ = $(SRC:.c=.o)

ALL_TOOLS = tools/dts89_lit40_render tools/dts89_lod41_selftest tools/dts89_lighting40_selftest tools/dts89_normal_decode tools/dts89_texture39_selftest tools/dts89_texture_decode tools/dts89_shaderer_selftest tools/dts89_shaderer_audit tools/dts89_uv38_selftest tools/dts89_pose37_selftest tools/dts89_pose37_export tools/dts89_roundtrip tools/dts89_inspect tools/dts89_selftest tools/dts89_tscript_dump \
	tools/dts89_dsq_inspect tools/dts89_probe tools/dts89_emit_materials tools/dts89_emit_constructor tools/dts89_runtime_plan tools/dts89_validate tools/dts89_anim_plan tools/dts89_tri_dump

all: $(ALL_TOOLS)

tools/dts89_roundtrip: $(OBJ) examples/example_roundtrip.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) examples/example_roundtrip.c

tools/dts89_inspect: $(OBJ) tools/dts89_inspect.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_inspect.c

tools/dts89_selftest: $(OBJ) tools/dts89_selftest.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_selftest.c

tools/dts89_tscript_dump: $(OBJ) tools/dts89_tscript_dump.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_tscript_dump.c

tools/dts89_dsq_inspect: $(OBJ) tools/dts89_dsq_inspect.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_dsq_inspect.c

tools/dts89_probe: $(OBJ) tools/dts89_probe.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_probe.c

tools/dts89_emit_materials: $(OBJ) tools/dts89_emit_materials.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_emit_materials.c

tools/dts89_emit_constructor: $(OBJ) tools/dts89_emit_constructor.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_emit_constructor.c

tools/dts89_runtime_plan: $(OBJ) tools/dts89_runtime_plan.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_runtime_plan.c

clean:
	rm -f $(OBJ) $(ALL_TOOLS)


tools/dts89_validate: $(OBJ) tools/dts89_validate.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_validate.c

tools/dts89_anim_plan: $(OBJ) tools/dts89_anim_plan.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_anim_plan.c


tools/dts89_tri_dump: $(OBJ) tools/dts89_tri_dump.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_tri_dump.c

tools/dts89_pose37_export: $(OBJ) tools/dts89_pose37_export.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_pose37_export.c

tools/dts89_pose37_selftest: $(OBJ) tools/dts89_pose37_selftest.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_pose37_selftest.c

tools/dts89_uv38_selftest: $(OBJ) tools/dts89_uv38_selftest.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_uv38_selftest.c


tools/dts89_shaderer_selftest: $(OBJ) tools/dts89_shaderer_selftest.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_shaderer_selftest.c

tools/dts89_shaderer_audit: $(OBJ) tools/dts89_shaderer_audit.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_shaderer_audit.c


tools/dts89_texture39_selftest: $(OBJ) tools/dts89_texture39_selftest.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_texture39_selftest.c

tools/dts89_texture_decode: $(OBJ) tools/dts89_texture_decode.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_texture_decode.c


tools/dts89_normal_decode: $(OBJ) tools/dts89_normal_decode.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_normal_decode.c


tools/dts89_lighting40_selftest: $(OBJ) tools/dts89_lighting40_selftest.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_lighting40_selftest.c


tools/dts89_lit40_render: $(OBJ) tools/dts89_lit40_render.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_lit40_render.c


tools/dts89_lod41_selftest: $(OBJ) tools/dts89_lod41_selftest.c
	$(CC) $(CFLAGS) -o $@ $(OBJ) tools/dts89_lod41_selftest.c
