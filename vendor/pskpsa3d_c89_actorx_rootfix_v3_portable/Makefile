# GNU Make. Native Windows/MSYS2 can set EXEEXT=.exe explicitly if needed.
CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -Werror -pedantic-errors -O2
CPPFLAGS ?= -Iinclude
AR ?= ar
ARFLAGS = rcs
EXEEXT ?= $(if $(filter Windows_NT,$(OS)),.exe,)

LIB = libpskpsa3d.a
OBJ = build/pskpsa3d.o
INFO = pskpsa_info$(EXEEXT)
SKIN_DUMP = pskpsa_skin_dump$(EXEEXT)
TEST_MINIMAL = test_minimal$(EXEEXT)
TEST_NUMERIC = test_numeric_limits$(EXEEXT)

all: $(LIB) $(INFO) $(SKIN_DUMP) $(TEST_MINIMAL) $(TEST_NUMERIC)

build:
	mkdir -p build

$(OBJ): src/pskpsa3d.c include/pskpsa3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/pskpsa3d.c -o $(OBJ)

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $(OBJ)

$(INFO): tools/pskpsa_info.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/pskpsa_info.c $(LIB) -o $(INFO)

$(SKIN_DUMP): tools/pskpsa_skin_dump.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/pskpsa_skin_dump.c $(LIB) -o $(SKIN_DUMP)

$(TEST_MINIMAL): tests/test_minimal.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_minimal.c $(LIB) -o $(TEST_MINIMAL)

$(TEST_NUMERIC): tests/test_numeric_limits.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_numeric_limits.c $(LIB) -o $(TEST_NUMERIC)

check: $(TEST_MINIMAL) $(TEST_NUMERIC)
	./$(TEST_MINIMAL)
	./$(TEST_NUMERIC)

clean:
	rm -rf build $(LIB) \
	  pskpsa_info pskpsa_skin_dump test_minimal test_numeric_limits \
	  pskpsa_info.exe pskpsa_skin_dump.exe test_minimal.exe test_numeric_limits.exe

.PHONY: all check clean
