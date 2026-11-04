#include "../gs3d_internal.h"
/* All generation vendors share this public surface. Explicit MDL7 material
   metadata is preserved; legacy types receive a neutral fallback. */
int gs3d_get_material(unsigned int group,unsigned int skin,GS3D_MaterialInfo *out){
    GS3D_Skin *s;
    if(!gs_is_open||!out||group>=gs3d_model.groups ||
       skin>=gs3d_model.group[group].skin_count)return GS3D_ERR_RANGE;
    s=&gs3d_model.group[group].skins[skin];
    out->is_explicit=s->has_material;
    out->has_image=(unsigned char)(s->type==6U || s->type==7U ||
              s->type==1U || (s->width && s->height));
    if(s->has_material){
        out->diffuse[0]=s->diffuse_q16[0];out->diffuse[1]=s->diffuse_q16[1];
        out->diffuse[2]=s->diffuse_q16[2];out->opacity=s->opacity_q16;
    }else{
        out->diffuse[0]=out->diffuse[1]=out->diffuse[2]=65536L;
        out->opacity=65536L;
    }
    return GS3D_OK;
}
