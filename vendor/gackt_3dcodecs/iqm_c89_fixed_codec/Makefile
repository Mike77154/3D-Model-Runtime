CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra

all: test_iqm_minimal test_iqm_math

test_iqm_minimal: src/iqm_fixed.c src/iqm_loader.c src/iqm_math.c tests/test_iqm_minimal.c
	$(CC) $(CFLAGS) -Isrc -o test_iqm_minimal src/iqm_fixed.c src/iqm_loader.c src/iqm_math.c tests/test_iqm_minimal.c

test_iqm_math: src/iqm_fixed.c src/iqm_loader.c src/iqm_math.c tests/test_iqm_math.c
	$(CC) $(CFLAGS) -Isrc -o test_iqm_math src/iqm_fixed.c src/iqm_loader.c src/iqm_math.c tests/test_iqm_math.c

check: test_iqm_minimal test_iqm_math
	./test_iqm_minimal
	./test_iqm_math

clean:
	rm -f test_iqm_minimal test_iqm_minimal.exe test_iqm_math test_iqm_math.exe
