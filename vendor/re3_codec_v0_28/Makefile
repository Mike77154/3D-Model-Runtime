CC ?= cc
AR ?= ar
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Werror -O2
CPPFLAGS ?= -Iinclude

all: re3emd_damage_prefix_audit test_damage_prefix re3emd_damage_contract test_damage_contract re3emd_damage_channels re3emd_damage_stress re3emd_damage_compare test_damage_families test_zombie_damage test_vram_composite re3emd_damage_audit re3packed_special_audit re3packed_info re3packed_preview re3packed_motion_audit re3emd_weighted_morph_audit test_pc_morph_blend test_pc_rebase re3emd_pc_rebase_audit re3emd_morph_space_audit test_morph_delta re3emd_clip_catalog re3emd_compare_clips re3scd_parts_probe test_parts_set re3emd_morph_audit re3emd_fidelity_audit libre3_codec.a re3emd_motion_audit test_animation re3emd_anim_preview re3emd_info re3emd_viewer re3emd_preview re3emd_reference_audit re3emd_parts re3emd_morph test_synthetic test_tim test_key_resolution test_texture_binding test_morph_retarget test_visual_state

re3_codec.o: src/re3_codec.c include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c src/re3_codec.c -o $@

libre3_codec.a: re3_codec.o
	$(AR) rcs $@ re3_codec.o

re3emd_morph_audit: tools/re3emd_morph_audit.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_morph_audit.c libre3_codec.a -o $@

re3emd_fidelity_audit: tools/re3emd_fidelity_audit.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_fidelity_audit.c libre3_codec.a -o $@

re3emd_info: tools/re3emd_info.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_info.c libre3_codec.a -o $@

re3emd_viewer: tools/re3emd_viewer.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_viewer.c libre3_codec.a -o $@

re3emd_preview: tools/re3emd_preview.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_preview.c libre3_codec.a -o $@

re3emd_reference_audit: tools/re3emd_reference_audit.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_reference_audit.c libre3_codec.a -o $@

re3emd_parts: tools/re3emd_parts.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_parts.c libre3_codec.a -o $@

re3emd_morph: tools/re3emd_morph.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_morph.c libre3_codec.a -o $@

basic_example: examples/basic.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) examples/basic.c libre3_codec.a -o $@

test_synthetic: tests/test_synthetic.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_synthetic.c libre3_codec.a -o $@

test_key_resolution: tests/test_key_resolution.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_key_resolution.c libre3_codec.a -o $@

test_tim: tests/test_tim.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_tim.c libre3_codec.a -o $@

test_morph_retarget: tests/test_morph_retarget.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_morph_retarget.c libre3_codec.a -o $@

test_texture_binding: tests/test_texture_binding.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_texture_binding.c libre3_codec.a -o $@

test: test_damage_prefix test_damage_contract test_damage_families test_zombie_damage test_vram_composite test_packed_header test_pc_rebase test_synthetic test_tim test_key_resolution test_animation test_morph_retarget test_visual_state test_parts_set
	./test_damage_prefix
	./test_damage_contract
	./test_damage_families
	./test_zombie_damage
	./test_vram_composite
	./test_packed_header
	./test_pc_rebase
	./test_synthetic
	./test_tim
	./test_animation
	./test_morph_retarget
	./test_visual_state
	./test_parts_set

# Full retail sweeps need an external corpus and are intentionally explicit.
# Example: make test-corpus CORPUS_DIR=/path/to/EMD
test-corpus: re3emd_info test_key_resolution
	@test -n "$(CORPUS_DIR)" || (echo "set CORPUS_DIR to a directory containing .EMD files"; exit 2)
	@find "$(CORPUS_DIR)" -type f -name '*.EMD' -print | { \
		pass=0; fail=0; \
		while IFS= read -r f; do \
			if ./re3emd_info "$$f" >/dev/null && ./test_key_resolution "$$f" >/dev/null; then \
				pass=`expr $$pass + 1`; \
			else \
				echo "FAIL $$f"; fail=`expr $$fail + 1`; \
			fi; \
		done; \
		echo "corpus pass=$$pass fail=$$fail"; test $$fail -eq 0; \
	}

test-tim-corpus: test_tim
	@test -n "$(TIM_DIR)" || (echo "set TIM_DIR to a directory containing .TIM files"; exit 2)
	@find "$(TIM_DIR)" -type f -name '*.TIM' -print | { \
		pass=0; fail=0; \
		while IFS= read -r f; do \
			if ./test_tim "$$f" >/dev/null; then pass=`expr $$pass + 1`; \
			else echo "FAIL $$f"; fail=`expr $$fail + 1`; fi; \
		done; \
		echo "tim corpus pass=$$pass fail=$$fail"; test $$fail -eq 0; \
	}

# Pair each EMD with a same-basename TIM and sample every triangle/quad UV corner.
test-texture-corpus: test_texture_binding
	@test -n "$(CORPUS_DIR)" || (echo "set CORPUS_DIR to paired .EMD/.TIM files"; exit 2)
	@find "$(CORPUS_DIR)" -type f -name '*.EMD' -print | { \
		pass=0; fail=0; skip=0; \
		while IFS= read -r f; do \
			t=`printf '%s\n' "$$f" | sed 's/\.EMD$$/.TIM/'`; \
			if test ! -f "$$t"; then skip=`expr $$skip + 1`; continue; fi; \
			if ./test_texture_binding "$$f" "$$t" >/dev/null; then pass=`expr $$pass + 1`; \
			else echo "FAIL $$f + $$t"; fail=`expr $$fail + 1`; fi; \
		done; \
		echo "texture pairs pass=$$pass fail=$$fail skip=$$skip"; test $$fail -eq 0; \
	}

clean:
	rm -f re3emd_damage_prefix_audit test_damage_prefix
	rm -f re3emd_damage_contract test_damage_contract
	rm -f re3emd_damage_compare re3emd_damage_stress re3emd_damage_channels test_damage_families
	rm -f test_zombie_damage test_vram_composite re3emd_damage_audit re3packed_special_audit
	rm -f test_packed_header re3packed_info re3packed_preview re3packed_motion_audit re3_packed.o re3emd_weighted_morph_audit test_pc_morph_blend test_pc_rebase re3emd_pc_rebase_audit re3emd_morph_space_audit test_morph_delta re3emd_clip_catalog re3emd_compare_clips re3emd_morph_audit re3emd_fidelity_audit re3emd_motion_audit test_animation re3emd_anim_preview libre3_codec.a re3_codec.o re3emd_info re3emd_viewer re3emd_preview re3emd_reference_audit re3emd_parts re3emd_morph basic_example test_synthetic test_tim test_key_resolution test_texture_binding test_synthetic_clang test_synthetic_ubsan test_key_resolution_ubsan test_morph_retarget test_visual_state test_parts_set re3scd_parts_probe

# v0.16 animation preview (generated BMP keyframes, no windowing dependencies)
re3emd_anim_preview: tools/re3emd_anim_preview.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_anim_preview.c libre3_codec.a -o $@

re3emd_motion_audit: tools/re3emd_motion_audit.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_motion_audit.c libre3_codec.a -o $@

test_animation: tests/test_animation.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_animation.c libre3_codec.a -o $@

# Explicit external EMD corpus audit. Existing resources are read-only.
test-animation-corpus: re3emd_motion_audit
	@test -n "$(CORPUS_DIR)" || (echo "set CORPUS_DIR to a directory of .EMD files"; exit 2)
	@find "$(CORPUS_DIR)" -type f -name '*.EMD' -print | { \
		pass=0; fail=0; \
		while IFS= read -r f; do \
			if ./re3emd_motion_audit "$$f" >/dev/null; then pass=`expr $$pass + 1`; \
			else echo "FAIL $$f"; fail=`expr $$fail + 1`; fi; \
		done; \
		echo "animation corpus pass=$$pass fail=$$fail"; test $$fail -eq 0; \
	}

# v0.19 state machine tests
test_visual_state: tests/test_visual_state.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_visual_state.c libre3_codec.a -o $@

re3scd_parts_probe: tools/re3scd_parts_probe.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3scd_parts_probe.c libre3_codec.a -o $@

test_parts_set: tests/test_parts_set.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_parts_set.c libre3_codec.a -o $@

# v0.20: exhaustive EDD/EMR clip listing and model-family pose comparator
re3emd_clip_catalog: tools/re3emd_clip_catalog.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_clip_catalog.c libre3_codec.a -o $@

re3emd_compare_clips: tools/re3emd_compare_clips.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_compare_clips.c libre3_codec.a -o $@

# v0.21: experimental morph coordinate-space interpretation and audit
re3emd_morph_space_audit: tools/re3emd_morph_space_audit.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_morph_space_audit.c libre3_codec.a -o $@

test_morph_delta: tests/test_morph_delta.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_morph_delta.c libre3_codec.a -o $@

# v0.22: Ghidra-confirmed original RE3 PC polygon record rebasing
re3emd_pc_rebase_audit: tools/re3emd_pc_rebase_audit.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_pc_rebase_audit.c libre3_codec.a -o $@
test_pc_rebase: tests/test_pc_rebase.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_pc_rebase.c libre3_codec.a -o $@

# v0.22 weighted binary01 morph blending (from Ghidra x86 routine)
test_pc_morph_blend: tests/test_pc_morph_blend.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_pc_morph_blend.c libre3_codec.a -o $@

# v0.23: quantitative weighted-morph audit of every vertex and mixed streams.
re3emd_weighted_morph_audit: tools/re3emd_weighted_morph_audit.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_weighted_morph_audit.c libre3_codec.a -o $@

# External resources must be supplied by the caller (not distributed).
test-weighted-corpus: re3emd_weighted_morph_audit
	@test -n "$(CORPUS_DIR)" || (echo "set CORPUS_DIR to an extracted EMD corpus"; exit 2)
	@find "$(CORPUS_DIR)" -type f -name '*.EMD' -print | { \
		pass=0; fail=0; \
		while IFS= read -r f; do \
			if ./re3emd_weighted_morph_audit "$$f" >/dev/null; then pass=`expr $$pass + 1`; \
			else echo "FAIL $$f"; fail=`expr $$fail + 1`; fi; \
		done; \
		echo "weighted-morph corpus pass=$$pass fail=$$fail"; test $$fail -eq 0; \
	}

# v0.24: PLD/PLW packed asset support (separate from enemy EMD parser).
re3_packed.o: src/re3_packed.c include/re3_packed.h include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c src/re3_packed.c -o $@
re3packed_info: tools/re3packed_info.c re3_packed.o libre3_codec.a
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3packed_info.c re3_packed.o libre3_codec.a -o $@
re3packed_preview: tools/re3packed_preview.c re3_packed.o libre3_codec.a
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3packed_preview.c re3_packed.o libre3_codec.a -o $@

re3packed_motion_audit: tools/re3packed_motion_audit.c re3_packed.o libre3_codec.a
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3packed_motion_audit.c re3_packed.o libre3_codec.a -o $@

test_packed_header: tests/test_packed_header.c re3_packed.o libre3_codec.a
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_packed_header.c re3_packed.o libre3_codec.a -o $@

# v0.25: shared VRAM, guarded zombie damage and special-asset diagnostics.
test_vram_composite: tests/test_vram_composite.c re3_packed.o libre3_codec.a
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_vram_composite.c re3_packed.o libre3_codec.a -o $@
test_zombie_damage: tests/test_zombie_damage.c libre3_codec.a
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_zombie_damage.c libre3_codec.a -o $@
re3emd_damage_audit: tools/re3emd_damage_audit.c libre3_codec.a
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_damage_audit.c libre3_codec.a -o $@
re3packed_special_audit: tools/re3packed_special_audit.c re3_packed.o libre3_codec.a
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3packed_special_audit.c re3_packed.o libre3_codec.a -o $@

re3emd_damage_compare: tools/re3emd_damage_compare.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_damage_compare.c libre3_codec.a -o $@

test_damage_families: tests/test_damage_families.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_damage_families.c libre3_codec.a -o $@

re3emd_damage_stress: tools/re3emd_damage_stress.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_damage_stress.c libre3_codec.a -o $@

re3emd_damage_channels: tools/re3emd_damage_channels.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_damage_channels.c libre3_codec.a -o $@

# v0.27: damage header-vs-skeleton contract.
re3emd_damage_contract: tools/re3emd_damage_contract.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_damage_contract.c libre3_codec.a -o $@

test_damage_contract: tests/test_damage_contract.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_damage_contract.c libre3_codec.a -o $@

# v0.28 static-sampler prefix evaluation
re3emd_damage_prefix_audit: tools/re3emd_damage_prefix_audit.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/re3emd_damage_prefix_audit.c libre3_codec.a -o $@

test_damage_prefix: tests/test_damage_prefix.c libre3_codec.a include/re3_codec.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_damage_prefix.c libre3_codec.a -o $@
