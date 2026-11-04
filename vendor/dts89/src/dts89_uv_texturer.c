#include "dts89_modules.h"
int dts89_uv_texturer_convert(const dts89_fx_vec2 *in_uv,int convention,
 dts89_fx_vec2 *out_uv)
{
 dts89_fx_vec2 result;
 if(!in_uv || !out_uv) return DTS89_ERR_BAD_ARG;
 if(convention!=DTS89_UV_TORQUE_TOP_LEFT &&
    convention!=DTS89_UV_OBJ_BOTTOM_LEFT) return DTS89_ERR_BAD_ARG;
 result=*in_uv;
 if(convention==DTS89_UV_OBJ_BOTTOM_LEFT){
  /* Retain original UV domain: no wrapping/clamping in the codec. */
  if(result.y < (-2147483647-1)+DTS89_FX_ONE) return DTS89_ERR_RANGE;
  result.y=DTS89_FX_ONE-result.y;
 }
 *out_uv=result;
 return DTS89_OK;
}
int dts89_uv_texturer_get(const dts89_mesh_view *mesh,dts89_s32 layer,
 dts89_s32 index,int convention,dts89_fx_vec2 *uv)
{
 dts89_fx_vec2 raw;
 int rc;
 if(!mesh || !uv) return DTS89_ERR_BAD_ARG;
 rc=dts89_mesh_get_fx_tvert(mesh,layer,index,&raw);
 if(rc!=DTS89_OK) return rc;
 return dts89_uv_texturer_convert(&raw,convention,uv);
}
int dts89_uv_texturer_sample(const dts89_mesh_view *mesh,dts89_s32 index,
 dts89_fx_vec2 *uv)
{
 return dts89_uv_texturer_get(mesh,0,index,DTS89_UV_TORQUE_TOP_LEFT,uv);
}
