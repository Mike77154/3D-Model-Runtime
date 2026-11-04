CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Werror -O2
SOURCES = fraginspect.c vendor_dbo/dbo_vendor.c vendor_fragmotion/frag_probe.c vendor_fragmotion/ugh_forensics.c vendor_fragmotion/arg_vendor.c vendor_blade3d/xmdl_vendor.c vendor_blade3d/xir_vendor.c vendor_directx_x/x_vendor.c vendor_common/fixed_matrix.c vendor_common/fixed_quat.c vendor_common/skin_fixed.c vendor_gamestudio/mdl_core.c vendor_gamestudio/mdl7_color.c vendor_gamestudio/gs_dispatch.c vendor_gamestudio/a3/mdl2_probe.c vendor_gamestudio/a4/mdl3.c vendor_gamestudio/a5/mdl45.c vendor_gamestudio/a6_a8/mdl7.c vendor_gamestudio/a6_a8/mdl7_rig.c
all: fraginspect ughcompare ughresync ughstructures ughgraph ughnativeprobe ughtree
fraginspect: $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@
ughcompare: ughcompare.c vendor_fragmotion/ugh_forensics.c
	$(CC) $(CFLAGS) ughcompare.c vendor_fragmotion/ugh_forensics.c -o $@
ughresync: ughresync.c
	$(CC) $(CFLAGS) ughresync.c -o $@
ughstructures: ughstructures.c
	$(CC) $(CFLAGS) ughstructures.c -o $@
ughgraph: ughgraph.c
	$(CC) $(CFLAGS) ughgraph.c -o $@
ughnativeprobe: ughnativeprobe.c
	$(CC) $(CFLAGS) ughnativeprobe.c -o $@
ughtree: ughtree.c
	$(CC) $(CFLAGS) ughtree.c -o $@
test: fraginspect ughcompare ughresync ughstructures ughgraph ughnativeprobe ughtree
	sh tests/run_tests_r4.sh
	sh tests/run_tests_r5.sh
	sh tests/run_tests_r6.sh
	sh tests/run_tests_r7.sh
	sh tests/run_tests_r8.sh
	sh tests/run_tests_r9.sh
	sh tests/run_tests_r10.sh
	sh tests/run_tests_r11.sh
	sh tests/run_tests_r12.sh
	sh tests/run_tests_r13.sh
	sh tests/run_tests_r14.sh
	sh tests/run_tests_r15.sh
	sh tests/run_tests_r16.sh
	sh tests/run_tests_r17.sh
	sh tests/run_tests_r18.sh
	sh tests/run_tests_r19.sh
	sh tests/run_tests_r21.sh
	sh tests/run_tests_r22.sh
	$(CC) $(CFLAGS) tests/test_mdl7_color.c vendor_gamestudio/mdl7_color.c -o test_color_r11
	./test_color_r11
	rm -f test_color_r11
clean:
	rm -f fraginspect ughcompare ughresync ughstructures ughgraph ughnativeprobe ughtree
