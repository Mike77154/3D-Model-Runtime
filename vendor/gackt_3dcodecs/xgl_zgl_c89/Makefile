CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2

all: xgl3d_demo

xgl3d_demo: demo/main.c src/xgl3d.c src/xgl3d_inflate.c src/xgl3d_obj.c src/xgl3d_extra.c src/xgl3d.h
	$(CC) $(CFLAGS) -o xgl3d_demo demo/main.c src/xgl3d.c src/xgl3d_inflate.c src/xgl3d_obj.c src/xgl3d_extra.c

clean:
	rm -f xgl3d_demo xgl3d_demo.exe
