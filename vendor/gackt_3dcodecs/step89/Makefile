CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude -Iext/cad_kernel -Iext/validator -Iext/complete_cad -Iext/xpress_vm
AR ?= ar
ARFLAGS ?= rcs

LIB = libstep89.a
OBJS = src/step89.o src/step89_geom.o src/step89_write.o \
       ext/cad_kernel/step89_cad_kernel.o \
       ext/validator/step89_validate.o \
       ext/complete_cad/step89_complete.o \
       ext/xpress_vm/step89_xpress_vm.o

all: $(LIB) examples/step_info examples/step_cad_validate examples/step_complete_scan examples/step_xpress_full_scan tests/test_step89 tests/test_step89_ext tests/test_step89_complete tests/test_step89_xpress_vm

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

src/step89.o: src/step89.c include/step89.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/step89.c -o src/step89.o

src/step89_geom.o: src/step89_geom.c include/step89.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/step89_geom.c -o src/step89_geom.o

src/step89_write.o: src/step89_write.c include/step89.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/step89_write.c -o src/step89_write.o

ext/cad_kernel/step89_cad_kernel.o: ext/cad_kernel/step89_cad_kernel.c ext/cad_kernel/step89_cad_kernel.h include/step89.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c ext/cad_kernel/step89_cad_kernel.c -o ext/cad_kernel/step89_cad_kernel.o

ext/validator/step89_validate.o: ext/validator/step89_validate.c ext/validator/step89_validate.h ext/cad_kernel/step89_cad_kernel.h include/step89.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c ext/validator/step89_validate.c -o ext/validator/step89_validate.o

ext/complete_cad/step89_complete.o: ext/complete_cad/step89_complete.c ext/complete_cad/step89_complete.h ext/validator/step89_validate.h ext/cad_kernel/step89_cad_kernel.h include/step89.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c ext/complete_cad/step89_complete.c -o ext/complete_cad/step89_complete.o

ext/xpress_vm/step89_xpress_vm.o: ext/xpress_vm/step89_xpress_vm.c ext/xpress_vm/step89_xpress_vm.h ext/complete_cad/step89_complete.h ext/validator/step89_validate.h ext/cad_kernel/step89_cad_kernel.h include/step89.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c ext/xpress_vm/step89_xpress_vm.c -o ext/xpress_vm/step89_xpress_vm.o

examples/step_info: examples/step_info.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) examples/step_info.c $(LIB) -o examples/step_info

examples/step_cad_validate: examples/step_cad_validate.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) examples/step_cad_validate.c $(LIB) -o examples/step_cad_validate

examples/step_complete_scan: examples/step_complete_scan.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) examples/step_complete_scan.c $(LIB) -o examples/step_complete_scan

examples/step_xpress_full_scan: examples/step_xpress_full_scan.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) examples/step_xpress_full_scan.c $(LIB) -o examples/step_xpress_full_scan

tests/test_step89: tests/test_step89.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_step89.c $(LIB) -o tests/test_step89

tests/test_step89_ext: tests/test_step89_ext.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_step89_ext.c $(LIB) -o tests/test_step89_ext

tests/test_step89_complete: tests/test_step89_complete.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_step89_complete.c $(LIB) -o tests/test_step89_complete

tests/test_step89_xpress_vm: tests/test_step89_xpress_vm.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_step89_xpress_vm.c $(LIB) -o tests/test_step89_xpress_vm

check: tests/test_step89 tests/test_step89_ext tests/test_step89_complete tests/test_step89_xpress_vm examples/step_info examples/step_cad_validate examples/step_complete_scan examples/step_xpress_full_scan
	./tests/test_step89
	./tests/test_step89_ext
	./tests/test_step89_complete
	./tests/test_step89_xpress_vm
	./examples/step_info samples/minimal.stp
	./examples/step_cad_validate samples/closed_triangle.stp
	./examples/step_complete_scan samples/ap242_pmi_seed.stp
	./examples/step_xpress_full_scan samples/ap242_pmi_seed.stp samples/ap242_express_seed.exp

clean:
	rm -f src/*.o ext/cad_kernel/*.o ext/validator/*.o ext/complete_cad/*.o ext/xpress_vm/*.o $(LIB) examples/step_info examples/step_cad_validate examples/step_complete_scan examples/step_xpress_full_scan tests/test_step89 tests/test_step89_ext tests/test_step89_complete tests/test_step89_xpress_vm
