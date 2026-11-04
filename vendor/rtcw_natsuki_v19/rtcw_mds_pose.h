#ifndef RTCW_MDS_POSE_H
#define RTCW_MDS_POSE_H
#include "rtcw_codec.h"
/* Internal, independent MDS skeletal solver. The caller validates all byte spans.
   Fractions Q12 (0..4096), rotations 16-bit full turns, origins Q6,
   orthogonal matrices Q14. No ownership, no heap, no floats. */
typedef struct rtcw_pose_input_s {
    const unsigned char *raw;
    unsigned long size, frames_offset, frame_stride, frames, bones;
    long torso_parent;
    long parent[RTCW_MAX_BONES];
    long distance[RTCW_MAX_BONES];
    long torso_weight[RTCW_MAX_BONES];
    unsigned long flags[RTCW_MAX_BONES];
    unsigned long legs_a, legs_b, torso_a, torso_b;
    unsigned int legs_t, torso_t;
    unsigned short torso_yaw;
    long root_a[3], root_b[3];
} rtcw_pose_input;
typedef struct rtcw_pose_output_s {
    long axis[RTCW_MAX_BONES][3][3]; /* matrix[world][local] */
    long origin[RTCW_MAX_BONES][3];
} rtcw_pose_output;
int rtcw_pose_solve(const rtcw_pose_input *in,rtcw_pose_output *out);
#endif
