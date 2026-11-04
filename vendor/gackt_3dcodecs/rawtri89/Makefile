CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB = librawtri89.a
OBJ = src/rawtri89.o
TEST = test_rawtri89
TOOL = rawtri89_info

all: $(LIB) $(TEST) $(TOOL)

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $(OBJ)

src/rawtri89.o: src/rawtri89.c include/rawtri89.h
	$(CC) $(CFLAGS) -c src/rawtri89.c -o src/rawtri89.o

$(TEST): tests/test_rawtri89.c $(LIB)
	$(CC) $(CFLAGS) tests/test_rawtri89.c $(LIB) -o $(TEST)

$(TOOL): tools/rawtri89_info.c $(LIB)
	$(CC) $(CFLAGS) tools/rawtri89_info.c $(LIB) -o $(TOOL)

test: $(TEST)
	./$(TEST)

clean:
	rm -f src/*.o $(LIB) $(TEST) $(TOOL)

.PHONY: all test clean
