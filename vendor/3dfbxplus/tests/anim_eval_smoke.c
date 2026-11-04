#include <stdio.h>
#include <string.h>
#include "3dfbx.h"

int main(void)
{
    FbxAnimCurve *c;
    fbx_real v0;
    fbx_real v1;
    fbx_real v2;
    fbx_real v3;
    fbx_real vw;
    fbx_real vl;
    FbxNodeAnimTrack tr;
    FbxAnimCurve *layer_curve;

    c = fbx_anim_curve_create(2);
    if (!c) return 2;
    c->keys[0].time = FBX_REAL_ZERO;
    c->keys[0].value = FBX_REAL_ZERO;
    c->keys[0].interpolation = (unsigned char)FBX_ANIM_INTERP_LINEAR;
    c->keys[1].time = FBX_REAL_ONE;
    c->keys[1].value = fbx_real_from_int(10);
    c->keys[1].interpolation = (unsigned char)FBX_ANIM_INTERP_LINEAR;

    v0 = fbx_anim_curve_sample(c, fbx_real_from_ratio(1, 2));
    fbx_anim_curve_set_extrapolation(c,
                                     (unsigned char)FBX_ANIM_EXTRAP_LINEAR,
                                     (unsigned char)FBX_ANIM_EXTRAP_REPEAT);
    v1 = fbx_anim_curve_sample(c, -FBX_REAL_ONE);
    v2 = fbx_anim_curve_sample(c, fbx_real_from_ratio(3, 2));
    fbx_anim_curve_set_extrapolation(c,
                                     (unsigned char)FBX_ANIM_EXTRAP_CONSTANT,
                                     (unsigned char)FBX_ANIM_EXTRAP_MIRROR);
    v3 = fbx_anim_curve_sample(c, fbx_real_from_ratio(3, 2));

    fbx_anim_curve_destroy(c);

    c = fbx_anim_curve_create(2);
    if (!c) return 3;
    c->keys[0].time = FBX_REAL_ZERO;
    c->keys[0].value = FBX_REAL_ZERO;
    c->keys[0].interpolation = (unsigned char)FBX_ANIM_INTERP_CUBIC;
    c->keys[0].right_tangent = fbx_real_from_int(10);
    c->keys[0].right_weight = fbx_real_from_ratio(1, 3);
    c->keys[0].tangent_flags = 0x01000000u;
    c->keys[1].time = FBX_REAL_ONE;
    c->keys[1].value = fbx_real_from_int(10);
    c->keys[1].left_tangent = fbx_real_from_int(10);
    c->keys[1].left_weight = fbx_real_from_ratio(1, 3);
    c->keys[1].tangent_flags = 0x02000000u;
    vw = fbx_anim_curve_sample(c, fbx_real_from_ratio(1, 2));
    fbx_anim_curve_destroy(c);

    memset(&tr, 0, sizeof(tr));
    layer_curve = fbx_anim_curve_create(1);
    if (!layer_curve) return 4;
    layer_curve->keys[0].time = FBX_REAL_ZERO;
    layer_curve->keys[0].value = fbx_real_from_int(15);
    if (!fbx_anim_track_add_layered_curve(&tr, FBX_ANIM_CHANNEL_T_X, 1, FBX_REAL_HALF,
                                          (unsigned char)FBX_ANIM_LAYER_BLEND_ADDITIVE, 0, layer_curve)) {
        fbx_anim_curve_destroy(layer_curve);
        return 5;
    }
    vl = fbx_anim_track_sample_channel(&tr, FBX_ANIM_CHANNEL_T_X, fbx_real_from_int(10), FBX_REAL_ZERO);
    fbx_anim_curve_destroy(layer_curve);
    fbx_release(tr.layered_channels[FBX_ANIM_CHANNEL_T_X]);

    printf("anim_eval OK: mid=%ld pre=%ld repeat=%ld mirror=%ld weighted=%ld layer=%ld\n",
           (long)v0, (long)v1, (long)v2, (long)v3, (long)vw, (long)vl);
    return 0;
}
