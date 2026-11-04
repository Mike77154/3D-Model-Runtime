CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -O2

all: demo export_roundtrip poser_report poser_prepare test

demo: examples/demo.c src/motion_bvh_poses.c src/motion_bvh_poses.h src/mbvh_trig_table.h
	$(CC) $(CFLAGS) examples/demo.c src/motion_bvh_poses.c -o demo_bvh

export_roundtrip: examples/export_roundtrip.c src/motion_bvh_poses.c src/motion_bvh_poses.h src/mbvh_trig_table.h
	$(CC) $(CFLAGS) examples/export_roundtrip.c src/motion_bvh_poses.c -o export_roundtrip

poser_report: examples/poser_report.c src/motion_bvh_poses.c src/motion_bvh_poses.h src/mbvh_trig_table.h
	$(CC) $(CFLAGS) examples/poser_report.c src/motion_bvh_poses.c -o poser_report

poser_prepare: examples/poser_prepare.c src/motion_bvh_poses.c src/motion_bvh_poses.h src/mbvh_trig_table.h
	$(CC) $(CFLAGS) examples/poser_prepare.c src/motion_bvh_poses.c -o poser_prepare

test: tests/test_motion_bvh_poses.c src/motion_bvh_poses.c src/motion_bvh_poses.h src/mbvh_trig_table.h
	$(CC) $(CFLAGS) tests/test_motion_bvh_poses.c src/motion_bvh_poses.c -o test_motion_bvh_poses
	./test_motion_bvh_poses

clean:
	rm -f demo_bvh export_roundtrip poser_report poser_prepare test_motion_bvh_poses
