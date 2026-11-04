CC = gcc
CFLAGS = -std=c89 -Wall -Wextra -pedantic -O2
AR = ar
ARFLAGS = rcs

LIBSRC = hlfp_common.c hlfp_smd.c hlfp_mdl.c hlfp_math.c hlfp_blend.c hlfp_compile.c hlfp_detach.c hlfp_ctrlquat.c hlfp_reemit.c hlfp_goldsrc.c hlfp_bundle.c hlfp_timeline.c hlfp_material89.c hlfp_goldsrc_shade89.c hlfp_perspective89.c hlfp_frustum89.c hlfp_subpixel89.c hlfp_transparency89.c hlfp_studiohalflife.c hlfp_softmdl.c hlfp_studiohalflife_plus.c hlfp_softmdl_plus.c hlfp_studiohalflife_ultra.c hlfp_softmdl_ultra.c
LIBOBJ = $(LIBSRC:.c=.o)

all: libhlfp.a test_hlfp test_hlfp_goldsrc test_studiohalflife test_studiohalflife_plus test_studiohalflife_ultra test_hlfp_corpusfix test_hlfp_timeline test_hlfp_material89 test_hlfp_v3 test_hlfp_v4 test_hlfp_v5 test_hlfp_v6 test_hlfp_v6_stress test_hlfp_v7 demo_subpixel_v7 corpus_probe_v7 demo_render_goldsrc_v7 demo_frustum_v6 demo_render_goldsrc_v5 demo_render_goldsrc_v6 demo_render_goldsrc_v3 demo_render_goldsrc_v4 demo_render_studiohalflife demo_render_studiohalflife_plus demo_render_studiohalflife_ultra demo_render_goldsrc_pose demo_render_goldsrc_subframe

libhlfp.a: $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $(LIBOBJ)

test_hlfp: test_hlfp.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ test_hlfp.c libhlfp.a

test_hlfp_goldsrc: test_hlfp_goldsrc.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ test_hlfp_goldsrc.c libhlfp.a

test_studiohalflife: test_studiohalflife.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ test_studiohalflife.c libhlfp.a

demo_render_studiohalflife: demo_render_studiohalflife.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ demo_render_studiohalflife.c libhlfp.a

test_studiohalflife_plus: test_studiohalflife_plus.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ test_studiohalflife_plus.c libhlfp.a

demo_render_studiohalflife_plus: demo_render_studiohalflife_plus.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ demo_render_studiohalflife_plus.c libhlfp.a

test_studiohalflife_ultra: test_studiohalflife_ultra.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ test_studiohalflife_ultra.c libhlfp.a

demo_render_studiohalflife_ultra: demo_render_studiohalflife_ultra.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ demo_render_studiohalflife_ultra.c libhlfp.a


test_hlfp_corpusfix: test_hlfp_corpusfix.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ test_hlfp_corpusfix.c libhlfp.a

bundle_probe: bundle_probe.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ bundle_probe.c libhlfp.a

smd_probe: smd_probe.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ smd_probe.c libhlfp.a

corpus_probe_full: corpus_probe_full.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ corpus_probe_full.c libhlfp.a

demo_render_goldsrc_pose: demo_render_goldsrc_pose.c libhlfp.a
	$(CC) $(CFLAGS) -o $@ demo_render_goldsrc_pose.c libhlfp.a

studiohalflife_preview.ppm: demo_render_studiohalflife
	./demo_render_studiohalflife

studiohalflife_plus_preview.tga: demo_render_studiohalflife_plus
	./demo_render_studiohalflife_plus

studiohalflife_ultra_preview.tga: demo_render_studiohalflife_ultra
	./demo_render_studiohalflife_ultra

test_output.txt: test_hlfp test_hlfp_goldsrc test_studiohalflife test_studiohalflife_plus test_studiohalflife_ultra test_hlfp_corpusfix test_hlfp_timeline
	./test_hlfp > test_output.txt
	./test_hlfp_goldsrc >> test_output.txt
	./test_studiohalflife >> test_output.txt
	./test_studiohalflife_plus >> test_output.txt
	./test_studiohalflife_ultra >> test_output.txt
	./test_hlfp_corpusfix >> test_output.txt
	./test_hlfp_timeline >> test_output.txt
	./test_hlfp_material89 >> test_output.txt

clean:
	rm -f $(LIBOBJ) libhlfp.a test_hlfp test_hlfp_goldsrc test_studiohalflife test_studiohalflife_plus test_studiohalflife_ultra test_hlfp_corpusfix test_hlfp_timeline test_hlfp_material89 test_hlfp_v3 test_hlfp_v4 test_hlfp_v5 test_hlfp_v6 test_hlfp_v6_stress test_hlfp_v7 demo_subpixel_v7 corpus_probe_v7 demo_render_goldsrc_v7 demo_frustum_v6 demo_render_goldsrc_v5 demo_render_goldsrc_v6 corpus_probe_v5 demo_render_goldsrc_v4 corpus_probe_v4 corpus_probe_v3 demo_render_goldsrc_v3 corpus_probe_material89 bundle_probe smd_probe corpus_probe_full demo_render_studiohalflife demo_render_studiohalflife_plus demo_render_studiohalflife_ultra demo_render_goldsrc_pose demo_render_goldsrc_subframe test_output.txt studiohalflife_preview.ppm studiohalflife_plus_preview.tga studiohalflife_ultra_preview.tga

test_hlfp_timeline: test_hlfp_timeline.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ test_hlfp_timeline.c libhlfp.a

corpus_probe_timeline: corpus_probe_timeline.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ corpus_probe_timeline.c libhlfp.a

test_hlfp_material89: test_hlfp_material89.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ test_hlfp_material89.c libhlfp.a

corpus_probe_material89: corpus_probe_material89.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ corpus_probe_material89.c libhlfp.a

demo_render_goldsrc_subframe: demo_render_goldsrc_subframe.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ demo_render_goldsrc_subframe.c libhlfp.a

test_hlfp_v3: test_hlfp_v3.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ test_hlfp_v3.c libhlfp.a

corpus_probe_v3: corpus_probe_v3.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ corpus_probe_v3.c libhlfp.a

demo_render_goldsrc_v3: demo_render_goldsrc_v3.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ demo_render_goldsrc_v3.c libhlfp.a


test_hlfp_v4: test_hlfp_v4.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ test_hlfp_v4.c libhlfp.a

demo_render_goldsrc_v4: demo_render_goldsrc_v4.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ demo_render_goldsrc_v4.c libhlfp.a

corpus_probe_v4: corpus_probe_v4.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ corpus_probe_v4.c libhlfp.a


test_hlfp_v5: test_hlfp_v5.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ test_hlfp_v5.c libhlfp.a

corpus_probe_v5: corpus_probe_v5.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ corpus_probe_v5.c libhlfp.a


demo_render_goldsrc_v5: demo_render_goldsrc_v5.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ demo_render_goldsrc_v5.c libhlfp.a


test_hlfp_v6: test_hlfp_v6.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ test_hlfp_v6.c libhlfp.a

corpus_probe_v6: corpus_probe_v6.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ corpus_probe_v6.c libhlfp.a


demo_render_goldsrc_v6: demo_render_goldsrc_v6.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ demo_render_goldsrc_v6.c libhlfp.a

demo_frustum_v6: demo_frustum_v6.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ demo_frustum_v6.c libhlfp.a

test_hlfp_v6_stress: test_hlfp_v6_stress.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ test_hlfp_v6_stress.c libhlfp.a


test_hlfp_v7: test_hlfp_v7.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ test_hlfp_v7.c libhlfp.a

demo_subpixel_v7: demo_subpixel_v7.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ demo_subpixel_v7.c libhlfp.a

corpus_probe_v7: corpus_probe_v7.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ corpus_probe_v7.c libhlfp.a

demo_render_goldsrc_v7: demo_render_goldsrc_v7.c libhlfp.a
	$(CC) $(CFLAGS) -Werror -o $@ demo_render_goldsrc_v7.c libhlfp.a
