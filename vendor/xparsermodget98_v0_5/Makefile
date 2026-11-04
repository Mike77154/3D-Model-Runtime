CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2
INCLUDES = -Iinclude -Isrc

SRC = src/xp_mszip_stream.c src/third_party/puff.c src/xp_reader.c src/xp_lexer.c src/xp_blexer.c src/xp_parser.c src/xp_templates.c src/xp_mszip.c src/xp_semantics.c src/xp_anim_pose.c
EXE = xp_dump
TEST_EXE = test_v04
SEM_TEST_EXE = test_v04_semantics
POSE_TEST_EXE = test_v05_poses

all: $(EXE)

$(EXE): $(SRC) examples/xp_dump.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

clean:
	rm -f $(EXE) $(TEST_EXE) $(SEM_TEST_EXE) $(POSE_TEST_EXE) *.o src/*.o src/third_party/*.o examples/*.o

$(TEST_EXE): $(SRC) tests/test_v04.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(SEM_TEST_EXE): $(SRC) tests/test_v04_semantics.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test-v04: $(TEST_EXE) $(SEM_TEST_EXE)
	./$(TEST_EXE)
	./$(SEM_TEST_EXE)

$(POSE_TEST_EXE): $(SRC) tests/test_v05_poses.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test-v05: $(TEST_EXE) $(SEM_TEST_EXE) $(POSE_TEST_EXE)
	./$(TEST_EXE)
	./$(SEM_TEST_EXE)
	./$(POSE_TEST_EXE)
