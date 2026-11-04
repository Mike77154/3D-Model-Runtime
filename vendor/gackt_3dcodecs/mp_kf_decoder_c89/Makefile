CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2
INCLUDES = -Iinclude
SRC = src/mpk_reader.c src/mpk_fixed.c

all: mpk_dump

mpk_dump: $(SRC) tools/mpk_dump.c
	$(CC) $(CFLAGS) $(INCLUDES) -o mpk_dump $(SRC) tools/mpk_dump.c

clean:
	rm -f mpk_dump mpk_dump.exe *.obj
