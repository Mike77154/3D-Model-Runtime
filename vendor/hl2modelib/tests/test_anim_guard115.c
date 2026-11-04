/* Reproduces extreme Source local translation values without external files.
   The opt-in guard must not modify a valid animation, or the native API. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_mdl m;
static hl2m_pose_matrices native,clean;
static hl2m_native_anim_report89 rep;
static hl2m_anim_safety_report89 safety;
static hl2m_u8 bytes[128];
static void set_up(void)
{
    memset(&m,0,sizeof(m));memset(bytes,0,sizeof(bytes));
    m.bone_count=1;m.anim_count=1;m.bones[0].parent=-1;
    m.bones[0].quat.w=HL2M_FIXED_ONE;
    m.bones[0].pos.x=15*HL2M_FIXED_ONE;
    m.anims[0].numframes=1;m.anims[0].first_anim_offset=32;
    strcpy(m.bones[0].name,"test_bone");
    bytes[32]=0;bytes[33]=4;/* ANIMPOS, no more records */
    bytes[36]=6;bytes[42]=1;bytes[43]=1;
    bytes[44]=0x9a;bytes[45]=0xd9;/* -9830 RLE */
}
int main(void)
{
    int r;
    set_up();
    /* 1.407e15 is a float32 posscale found on Licker Tongue5. */
    m.bones[0].posscale_ieee32[0]=0x59a00000u;
    r=hl2m_master_anim89(bytes,sizeof(bytes),&m,0,0,&native,&rep);
    if(r){printf("FAIL raw %d\n",r);return 1;}
    r=hl2m_master_anim_guard89(bytes,sizeof(bytes),0,0,&m,0,0,
        512*HL2M_FIXED_ONE,&clean,&rep,&safety);
    if(r || safety.replaced_axes!=1 || safety.replaced_bones!=1 || safety.first_bone!=0 ||
       clean.local[0].m[0][3]!=15*HL2M_FIXED_ONE){
        printf("FAIL guard RLE %d axes %d bones %d local %d\n",r,safety.replaced_axes,safety.replaced_bones,clean.local[0].m[0][3]);return 2;
    }
    if(native.local[0].m[0][3]==clean.local[0].m[0][3])return 3;
    /* A valid compression scale must pass completely unchanged. */
    set_up();m.bones[0].posscale_ieee32[0]=0x3b800000u;/* 0.00390625 */
    r=hl2m_master_anim89(bytes,sizeof(bytes),&m,0,0,&native,&rep);
    if(r)return 4;
    r=hl2m_master_anim_guard89(bytes,sizeof(bytes),0,0,&m,0,0,
        512*HL2M_FIXED_ONE,&clean,&rep,&safety);
    if(r || safety.replaced_axes || native.local[0].m[0][3]!=clean.local[0].m[0][3])return 5;
    /* Hunter's RAWPOS contains half-float -65408, not a sensible
       local pelvis translation. Do not convert it into signed overflow. */
    set_up();bytes[33]=1;bytes[36]=0xfe;bytes[37]=0xfb;
    r=hl2m_master_anim_guard89(bytes,sizeof(bytes),0,0,&m,0,0,
        512*HL2M_FIXED_ONE,&clean,&rep,&safety);
    if(r || safety.replaced_axes!=1 || clean.local[0].m[0][3]!=15*HL2M_FIXED_ONE)return 6;
    puts("PASS guard115: oversized RLE, valid-scale invariance, raw Vector48 fallback");
    return 0;
}
