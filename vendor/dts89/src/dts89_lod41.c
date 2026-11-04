#include "dts89_lod41.h"
int dts89_lod41_orthographic_axis(const dts89_shape *shape,
 const dts89_runtime_shape *runtime, dts89_s32 up_axis,
 dts89_fx16_16 pixels_per_world_unit, dts89_detail_choice *choice,
 dts89_fx16_16 *projected_pixels)
{
 dts89_fx_vec3 lo,hi;
 dts89_fx16_16 extent,pix;
 int rc;
 if(!shape||!runtime||!choice||!projected_pixels ||
    up_axis<0||up_axis>2||pixels_per_world_unit<=0)
  return DTS89_ERR_BAD_ARG;
 rc=dts89_shape_get_fx_bounds(shape,&lo,&hi);
 if(rc)return rc;
 /* Reject invalid or excessively wide bounds BEFORE signed subtraction. */
 {
  dts89_fx16_16 min_axis,max_axis;
  if(up_axis==0){min_axis=lo.x;max_axis=hi.x;}
  else if(up_axis==1){min_axis=lo.y;max_axis=hi.y;}
  else {min_axis=lo.z;max_axis=hi.z;}
  if(max_axis<=min_axis)return DTS89_ERR_FORMAT;
  if(min_axis<0 && max_axis>0x7FFFFFFF+min_axis)
   return DTS89_ERR_RANGE;
  extent=max_axis-min_axis;
 }
 pix=dts89_fx_mul(extent,pixels_per_world_unit);
 if(pix<=0)return DTS89_ERR_RANGE;
 rc=dts89_runtime_select_detail_from_pixel_size(shape,runtime,pix,choice);
 if(rc)return rc;
 *projected_pixels=pix;
 return DTS89_OK;
}
