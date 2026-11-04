#include "dts89_lod41.h"
#include <stdio.h>
#include <string.h>
static int test_projected_bounds(void)
{
 dts89_shape shape;
 dts89_runtime_shape runtime;
 dts89_detail details[4];
 dts89_detail_choice choice;
 dts89_fx16_16 pixels;
 dts89_u32 v;
 int rc;
 memset(&shape,0,sizeof(shape));
 memset(&runtime,0,sizeof(runtime));
 memset(details,0,sizeof(details));
 /* Binary32 bits are serialized fields, not a native real-number type in the program. */
 details[0].size_bits=0x43A00000u; /* 320 */
 details[1].size_bits=0x43200000u; /* 160 */
 details[2].size_bits=0x42A00000u; /* 80 */
 details[3].size_bits=0x42200000u; /* 40 */
 shape.num_details=4;shape.details=details;
 /* Bounds min=0 max=2.0 along the Z axis. */
 shape.bounds_bits[0]=shape.bounds_bits[1]=shape.bounds_bits[2]=0u;
 v=0x40000000u;
 shape.bounds_bits[3]=v;shape.bounds_bits[4]=v;shape.bounds_bits[5]=v;
 rc=dts89_lod41_orthographic_axis(&shape,&runtime,2,dts89_fx_from_int(206),&choice,&pixels);
 if(rc || choice.detail_level!=0 || dts89_fx_to_int(pixels)!=412)return 1;
 rc=dts89_runtime_select_detail_from_pixel_size(&shape,&runtime,dts89_fx_from_int(64),&choice);
 if(rc || choice.detail_level!=3)return 2;
 rc=dts89_lod41_orthographic_axis(&shape,&runtime,3,dts89_fx_from_int(206),&choice,&pixels);
 if(rc!=DTS89_ERR_BAD_ARG)return 3;
 return 0;
}
int main(void)
{
 int rc=test_projected_bounds();
 if(rc){fprintf(stderr,"lod41 selftest failed=%d\n",rc);return 1;}
 puts("lod41 selftest ok");return 0;
}
