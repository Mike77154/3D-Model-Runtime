CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Werror -Iinclude
AR ?= ar
ARFLAGS ?= rcs

SRC = \
    src/bravure_bvh_utils.c \
    src/bravure_bvh_fixed.c \
    src/bravure_bvh_mat.c \
    src/bravure_bvh_quat.c \
    src/bravure_bvh_motion.c \
    src/bravure_bvh_filter.c \
    src/bravure_bvh_retarget.c \
    src/bravure_bvh_map.c \
    src/bravure_bvh_axis.c \
    src/bravure_bvh_parse.c \
    src/bravure_bvh_diag.c \
    src/bravure_bvh_pose.c \
    src/bravure_bvh_write.c \
    src/bravure_bvh_obj.c \
    src/bravure_bvh_report.c

OBJ = $(SRC:.c=.o)

all: libbravure_bvh.a bravure_bvh_demo bravure_bvh_tests

libbravure_bvh.a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

bravure_bvh_demo: $(OBJ) examples/bravure_bvh_demo.o
	$(CC) $(CFLAGS) -o $@ $(OBJ) examples/bravure_bvh_demo.o

bravure_bvh_tests: $(OBJ) tests/test_bravure_bvh.o
	$(CC) $(CFLAGS) -o $@ $(OBJ) tests/test_bravure_bvh.o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) examples/bravure_bvh_demo.o tests/test_bravure_bvh.o \
	      libbravure_bvh.a bravure_bvh_demo bravure_bvh_tests \
	      out_roundtrip.bvh out_pose.obj out_skeleton.dot out_motion.csv

run: bravure_bvh_demo
	./bravure_bvh_demo samples/simple.bvh out_roundtrip.bvh out_pose.obj out_skeleton.dot out_motion.csv

check: bravure_bvh_tests
	./bravure_bvh_tests
