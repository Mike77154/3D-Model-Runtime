CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
ARFLAGS ?= rcs

BUILD_DIR = build
LIB = $(BUILD_DIR)/libtmdcodec3d.a
OBJ_CORE = $(BUILD_DIR)/tmd_codec3d.o
OBJ_ANIM = $(BUILD_DIR)/tmd_anim3d.o
OBJ_HMD = $(BUILD_DIR)/tmd_hmd3d.o
OBJ_MIME = $(BUILD_DIR)/tmd_mime3d.o
OBJ_CAT3 = $(BUILD_DIR)/tmd_hmd_cat3.o
OBJ_CAT3_PLAY = $(BUILD_DIR)/tmd_hmd_cat3_play.o
OBJS = $(OBJ_CORE) $(OBJ_ANIM) $(OBJ_HMD) $(OBJ_MIME) $(OBJ_CAT3) $(OBJ_CAT3_PLAY)
SELFTEST = $(BUILD_DIR)/tmd_selftest
ANIMTEST = $(BUILD_DIR)/tmd_anim_selftest
DUMP = $(BUILD_DIR)/tmd_dump
TOOBJ = $(BUILD_DIR)/tmd_to_obj
TODDUMP = $(BUILD_DIR)/tod_dump
HMDDUMP = $(BUILD_DIR)/hmd_dump
CAT3DUMP = $(BUILD_DIR)/hmd_cat3_dump
HMDMIMETEST = $(BUILD_DIR)/tmd_hmd_mime_selftest
CAT3TEST = $(BUILD_DIR)/tmd_hmd_cat3_selftest
CAT3PLAYTEST = $(BUILD_DIR)/tmd_hmd_cat3_play_selftest

all: $(LIB) $(SELFTEST) $(ANIMTEST) $(HMDMIMETEST) $(CAT3TEST) $(CAT3PLAYTEST) $(DUMP) $(TOOBJ) $(TODDUMP) $(HMDDUMP) $(CAT3DUMP)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_CORE): src/tmd_codec3d.c include/tmd_codec3d.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/tmd_codec3d.c -o $(OBJ_CORE)

$(OBJ_ANIM): src/tmd_anim3d.c include/tmd_anim3d.h include/tmd_codec3d.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/tmd_anim3d.c -o $(OBJ_ANIM)

$(OBJ_HMD): src/tmd_hmd3d.c include/tmd_hmd3d.h include/tmd_codec3d.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/tmd_hmd3d.c -o $(OBJ_HMD)

$(OBJ_MIME): src/tmd_mime3d.c include/tmd_mime3d.h include/tmd_codec3d.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/tmd_mime3d.c -o $(OBJ_MIME)

$(OBJ_CAT3): src/tmd_hmd_cat3.c include/tmd_hmd_cat3.h include/tmd_hmd3d.h include/tmd_anim3d.h include/tmd_codec3d.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/tmd_hmd_cat3.c -o $(OBJ_CAT3)

$(OBJ_CAT3_PLAY): src/tmd_hmd_cat3_play.c include/tmd_hmd_cat3_play.h include/tmd_hmd_cat3.h include/tmd_hmd3d.h include/tmd_anim3d.h include/tmd_codec3d.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/tmd_hmd_cat3_play.c -o $(OBJ_CAT3_PLAY)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $(LIB) $(OBJS)

$(SELFTEST): tests/tmd_selftest.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/tmd_selftest.c $(OBJS) -o $(SELFTEST)

$(ANIMTEST): tests/tmd_anim_selftest.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/tmd_anim_selftest.c $(OBJS) -o $(ANIMTEST)

$(DUMP): tools/tmd_dump.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tools/tmd_dump.c $(OBJS) -o $(DUMP)

$(TOOBJ): tools/tmd_to_obj.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tools/tmd_to_obj.c $(OBJS) -o $(TOOBJ)

$(TODDUMP): tools/tod_dump.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tools/tod_dump.c $(OBJS) -o $(TODDUMP)

$(HMDDUMP): tools/hmd_dump.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tools/hmd_dump.c $(OBJS) -o $(HMDDUMP)

$(CAT3DUMP): tools/hmd_cat3_dump.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tools/hmd_cat3_dump.c $(OBJS) -o $(CAT3DUMP)

$(HMDMIMETEST): tests/tmd_hmd_mime_selftest.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/tmd_hmd_mime_selftest.c $(OBJS) -o $(HMDMIMETEST)

$(CAT3TEST): tests/tmd_hmd_cat3_selftest.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/tmd_hmd_cat3_selftest.c $(OBJS) -o $(CAT3TEST)

$(CAT3PLAYTEST): tests/tmd_hmd_cat3_play_selftest.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/tmd_hmd_cat3_play_selftest.c $(OBJS) -o $(CAT3PLAYTEST)

test: $(SELFTEST) $(ANIMTEST) $(HMDMIMETEST) $(CAT3TEST) $(CAT3PLAYTEST)
	$(SELFTEST)
	$(ANIMTEST)
	$(HMDMIMETEST)
	$(CAT3TEST)
	$(CAT3PLAYTEST)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all test clean
