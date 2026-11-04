CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2 -Iinclude
LDFLAGS ?= -lm

LIB = build/libgiffy_recodec.a
OBJ = build/giffy_recodec.o

all: $(LIB) build/grc build/giffy_re_view build/emd_textured_viewer build/emd_anim_table build/emd_anim_report

build:
	mkdir -p build

$(OBJ): src/giffy_recodec.c include/giffy_recodec.h | build
	$(CC) $(CFLAGS) -c src/giffy_recodec.c -o $(OBJ)

$(LIB): $(OBJ)
	$(AR) rcs $(LIB) $(OBJ)

build/grc: tools/grc.c $(LIB)
	$(CC) $(CFLAGS) tools/grc.c $(LIB) $(LDFLAGS) -o build/grc

build/giffy_re_view: viewer/giffy_re_view.c $(LIB)
	$(CC) $(CFLAGS) viewer/giffy_re_view.c $(LIB) $(LDFLAGS) -o build/giffy_re_view

clean:
	rm -rf build

.PHONY: all clean

build/emd_textured_viewer: tools/emd_textured_viewer.c $(LIB) include/giffy_recodec.h
	$(CC) $(CFLAGS) tools/emd_textured_viewer.c $(LIB) $(LDFLAGS) -o build/emd_textured_viewer


build/emd_anim_table: tools/emd_anim_table.c
	$(CC) $(CFLAGS) tools/emd_anim_table.c -o build/emd_anim_table


build/emd_anim_report: tools/emd_anim_report.c
	$(CC) $(CFLAGS) tools/emd_anim_report.c -o build/emd_anim_report
