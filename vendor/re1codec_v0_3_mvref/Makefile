CC ?= cc
CFLAGS ?= -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2
CPPFLAGS ?= -Iinclude
BUILD = build
LIBOBJ = $(BUILD)/re1codec.o

all: $(BUILD)/re1codec_inspect $(BUILD)/re1codec_mv_audit $(BUILD)/test_em1111 $(BUILD)/test_ft4_packet

$(BUILD):
	mkdir -p $(BUILD)

$(LIBOBJ): src/re1codec.c include/re1codec.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/re1codec.c -o $(LIBOBJ)

$(BUILD)/re1codec_inspect: tools/re1codec_inspect.c $(LIBOBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/re1codec_inspect.c $(LIBOBJ) -o $@

$(BUILD)/re1codec_mv_audit: tools/re1codec_mv_audit.c $(LIBOBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/re1codec_mv_audit.c $(LIBOBJ) -o $@

$(BUILD)/test_em1111: tests/test_em1111.c $(LIBOBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_em1111.c $(LIBOBJ) -o $@

$(BUILD)/test_ft4_packet: tests/test_ft4_packet.c $(LIBOBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_ft4_packet.c $(LIBOBJ) -o $@

check: all
	@test -n "$(EMD)" || (echo "usage: make check EMD=/path/EM1111.EMD"; exit 2)
	$(BUILD)/test_em1111 "$(EMD)"

clean:
	rm -rf $(BUILD)

.PHONY: all check clean
