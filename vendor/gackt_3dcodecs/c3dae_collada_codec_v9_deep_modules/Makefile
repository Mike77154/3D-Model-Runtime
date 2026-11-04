CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude
AR ?= ar

all: libc3dae.a c3dae_example c3zipx_probe c3dae_v7_probe c3dae_v8_probe c3dae_v9_probe

libc3dae.a: src/c3dae.o src/c3zae.o src/c3zipx.o src/c3far.o src/c3dae_v7.o src/c3dae_v8.o src/c3dae_v9.o
	$(AR) rcs $@ src/c3dae.o src/c3zae.o src/c3zipx.o src/c3far.o src/c3dae_v7.o src/c3dae_v8.o src/c3dae_v9.o

src/c3dae.o: src/c3dae.c include/c3dae.h
	$(CC) $(CFLAGS) -c src/c3dae.c -o src/c3dae.o

src/c3zae.o: src/c3zae.c include/c3zae.h include/c3dae.h
	$(CC) $(CFLAGS) -c src/c3zae.c -o src/c3zae.o

src/c3zipx.o: src/c3zipx.c include/c3zipx.h include/c3dae.h
	$(CC) $(CFLAGS) -c src/c3zipx.c -o src/c3zipx.o

src/c3far.o: src/c3far.c include/c3far.h include/c3dae.h
	$(CC) $(CFLAGS) -c src/c3far.c -o src/c3far.o

src/c3dae_v7.o: src/c3dae_v7.c include/c3dae_v7.h include/c3dae.h
	$(CC) $(CFLAGS) -c src/c3dae_v7.c -o src/c3dae_v7.o

src/c3dae_v8.o: src/c3dae_v8.c include/c3dae_v8.h include/c3dae.h
	$(CC) $(CFLAGS) -c src/c3dae_v8.c -o src/c3dae_v8.o

src/c3dae_v9.o: src/c3dae_v9.c include/c3dae_v9.h include/c3dae.h
	$(CC) $(CFLAGS) -c src/c3dae_v9.c -o src/c3dae_v9.o

c3dae_example: examples/test_main.c libc3dae.a
	$(CC) $(CFLAGS) examples/test_main.c libc3dae.a -o c3dae_example

c3zipx_probe: examples/test_zipx.c libc3dae.a
	$(CC) $(CFLAGS) examples/test_zipx.c libc3dae.a -o c3zipx_probe

c3dae_v7_probe: examples/test_v7.c libc3dae.a
	$(CC) $(CFLAGS) examples/test_v7.c libc3dae.a -o c3dae_v7_probe

c3dae_v8_probe: examples/test_v8.c libc3dae.a
	$(CC) $(CFLAGS) examples/test_v8.c libc3dae.a -o c3dae_v8_probe

c3dae_v9_probe: examples/test_v9.c libc3dae.a
	$(CC) $(CFLAGS) examples/test_v9.c libc3dae.a -o c3dae_v9_probe

clean:
	rm -f src/*.o libc3dae.a c3dae_example c3zipx_probe c3dae_v7_probe c3dae_v8_probe c3dae_v9_probe
