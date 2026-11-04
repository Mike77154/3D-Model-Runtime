CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra -pedantic -Iinclude

all: test_xsi_decoder test_xsi_advanced gxsi_file_dump

test_xsi_decoder: src/gxsi_decoder3d.c tests/test_xsi_decoder.c include/gxsi_decoder3d.h
	$(CC) $(CFLAGS) src/gxsi_decoder3d.c tests/test_xsi_decoder.c -o test_xsi_decoder

test_xsi_advanced: src/gxsi_decoder3d.c tests/test_xsi_advanced.c include/gxsi_decoder3d.h
	$(CC) $(CFLAGS) src/gxsi_decoder3d.c tests/test_xsi_advanced.c -o test_xsi_advanced

gxsi_file_dump: src/gxsi_decoder3d.c tools/gxsi_file_dump.c include/gxsi_decoder3d.h
	$(CC) $(CFLAGS) src/gxsi_decoder3d.c tools/gxsi_file_dump.c -o gxsi_file_dump

clean:
	rm -f test_xsi_decoder test_xsi_advanced gxsi_file_dump *.obj
