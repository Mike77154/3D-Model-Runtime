#include "dts89_pose37.h"

int dts89_animer_cursor(const dts89_sequence *seq,dts89_fx16_16 time,
                        dts89_sequence_cursor *out)
{ return dts89_sequence_select_keyframes(seq,time,out); }

/* One sequence/thread per pose. The detailed local/global transform work is
   intentionally isolated in posefeamw so other runtimes can drive it. */
int dts89_animer_evaluate(const dts89_shape *shape,dts89_s32 sequence_index,
                          dts89_fx16_16 position,dts89_fx_mat34 *global,
                          dts89_u32 capacity)
{ return dts89_pose37_evaluate(shape,sequence_index,position,global,capacity); }
