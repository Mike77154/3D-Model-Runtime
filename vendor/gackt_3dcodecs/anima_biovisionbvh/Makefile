CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB_OBJS = \
    src/anima_biovisionbvh_fixed.o \
    src/anima_biovisionbvh_lexer.o \
    src/anima_biovisionbvh_parse.o \
    src/anima_biovisionbvh_eval.o \
    src/anima_biovisionbvh_tools.o \
    src/anima_biovisionbvh_export.o \
    src/anima_biovisionbvh_extra.o \
    src/anima_biovisionbvh_profile.o \
    src/anima_biovisionbvh_resample.o

all: libanima_biovisionbvh.a examples/anima_bvh_info tests/test_anima_bvh

libanima_biovisionbvh.a: $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

src/%.o: src/%.c include/anima_biovisionbvh.h src/anima_biovisionbvh_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

examples/anima_bvh_info: examples/anima_bvh_info.c libanima_biovisionbvh.a
	$(CC) $(CFLAGS) examples/anima_bvh_info.c libanima_biovisionbvh.a -o $@

tests/test_anima_bvh: tests/test_anima_bvh.c libanima_biovisionbvh.a
	$(CC) $(CFLAGS) tests/test_anima_bvh.c libanima_biovisionbvh.a -o $@

check: tests/test_anima_bvh
	./tests/test_anima_bvh

sample: examples/anima_bvh_info
	./examples/anima_bvh_info examples/sample_minimal.bvh examples/sample_pose.obj 1 examples/sample_skeleton.dot examples/sample_channels.csv examples/sample_roundtrip.bvh examples/sample_bones.csv examples/sample_root_path.csv examples/sample_profile.csv

clean:
	rm -f $(LIB_OBJS) libanima_biovisionbvh.a examples/anima_bvh_info tests/test_anima_bvh examples/sample_pose.obj examples/sample_skeleton.dot examples/sample_channels.csv examples/sample_roundtrip.bvh examples/sample_bones.csv examples/sample_root_path.csv examples/sample_profile.csv

.PHONY: all check sample clean
