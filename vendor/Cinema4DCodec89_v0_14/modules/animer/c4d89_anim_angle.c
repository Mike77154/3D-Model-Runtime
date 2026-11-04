#include "c4d89.h"
#include <limits.h>
/* Optional SHORT-ARC interpolation for HPB radians represented as Q16.16.
   NOT for explicit multi-turn animation (use the original sampler there).
   This is value interpolation only, NOT Cinema 4D's Bezier tangents. */
#define C4D89_ANGLE_PI 205887L
#define C4D89_ANGLE_TWO_PI 411775L
#define C4D89_ANGLE_RANGE 6588400L
int c4d89_angle_lerp_q16(long left,long right,long alpha,long *out)
{
 long diff,scaled,value;
 int err;
 if(!out||alpha<0L||alpha>65536L)return C4D89_ERR_CORRUPT;
 /* Bound values conservatively to avoid arithmetic overflow on 32-bit long.
    Very large angles may encode intentional full spins, so reject them. */
 if(left<-C4D89_ANGLE_RANGE||left>C4D89_ANGLE_RANGE||
    right<-C4D89_ANGLE_RANGE||right>C4D89_ANGLE_RANGE)return C4D89_ERR_UNSUPPORTED;
 diff=right-left;
 while(diff>C4D89_ANGLE_PI)diff-=C4D89_ANGLE_TWO_PI;
 while(diff< -C4D89_ANGLE_PI)diff+=C4D89_ANGLE_TWO_PI;
 err=c4d89_fixed_mul(diff,alpha,&scaled);
 if(err)return err;
 value=left+scaled;
 /* Canonical [-pi,+pi] is not assumed. Return [0,2pi) for the renderer. */
 while(value<0L)value+=C4D89_ANGLE_TWO_PI;
 while(value>=C4D89_ANGLE_TWO_PI)value-=C4D89_ANGLE_TWO_PI;
 *out=value;return C4D89_OK;
}
