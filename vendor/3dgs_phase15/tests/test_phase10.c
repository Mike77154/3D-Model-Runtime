#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <limits.h>
#define CHECK(x) do { if(!(x)){printf("PHASE10 FAILED at line %d\n",__LINE__);return 1;} } while(0)
static long diff(long a,long b){return a>b?a-b:b-a;}
static int skeleton(void){
    GS3D_BoneMask mask;GS3D_LayerRequest req;GS3D_RootDelta root,old;
    GS3D_Clip base,over,moving;GS3D_Vec no_overlay,pose,other;
    unsigned int gen;int r;
    CHECK(gs3d_open("tests/fixtures/mdl7_skeletal_sequence.mdl")==GS3D_OK);
    CHECK(gs3d_model.bones==2U);
    CHECK(gs3d_mask_init(&mask,0L)==0);
    CHECK(gs3d_mask_set(&mask,1U,GS3D_Q16_ONE)==0);
    CHECK(gs3d_clip_create(&base,7U,0U,0U,1U,100UL,0U,2U)==0);
    CHECK(gs3d_clip_create(&over,7U,0U,1U,1U,100UL,0U,2U)==0);
    memset(&req,0,sizeof(req));req.base=base;req.overlay=over;
    req.overlay_weight_q16=65536L;
    CHECK(gs3d_vendor_layer_apply(7U,&req,&mask,0)==0);
    pose=gs3d_model.v[1];
    CHECK(diff(pose.x,98304L)<200L && diff(pose.y,32768L)<200L);
    CHECK(gs3d_model.v[2].y==65536L);
    CHECK(gs3d_vendor_layer_apply(7U,&req,&mask,0)==0);
    CHECK(gs3d_model.v[1].x==pose.x&&gs3d_model.v[1].y==pose.y);
    req.overlay_weight_q16=0L;
    CHECK(gs3d_layer_apply(&req,&mask,0)==0);no_overlay=gs3d_model.v[1];
    CHECK(diff(no_overlay.x,131072L)<50L&&diff(no_overlay.y,0L)<50L);
    req.overlay_weight_q16=32768L;
    CHECK(gs3d_layer_apply(&req,&mask,0)==0);
    CHECK(gs3d_model.v[1].x<no_overlay.x&&gs3d_model.v[1].x>pose.x);
    CHECK(gs3d_model.v[1].y>0L&&gs3d_model.v[1].y<pose.y);
    req.overlay_weight_q16=65536L;
    CHECK(gs3d_mask_set(&mask,1U,32768L)==0);
    CHECK(gs3d_layer_apply(&req,&mask,0)==0);
    CHECK(gs3d_model.v[1].x<no_overlay.x&&gs3d_model.v[1].x>pose.x);
    CHECK(gs3d_mask_set(&mask,1U,65536L)==0);
    CHECK(gs3d_mask_subtree(&mask,0U,0L)==0);
    CHECK(mask.weight_q16[0]==0L&&mask.weight_q16[1]==0L);
    CHECK(gs3d_mask_subtree(&mask,1U,65536L)==0);
    CHECK(mask.weight_q16[0]==0L&&mask.weight_q16[1]==65536L);
    CHECK(gs3d_mask_set(&mask,1U,-1L)==GS3D_ERR_RANGE);
    CHECK(gs3d_mask_set(&mask,1U,65537L)==GS3D_ERR_RANGE);
    CHECK(gs3d_mask_subtree(&mask,9U,1L)==GS3D_ERR_RANGE);
    mask.weight_q16[0]=-1L;
    CHECK(gs3d_layer_apply(&req,&mask,0)==GS3D_ERR_RANGE);
    mask.weight_q16[0]=0L;
    mask.bone_count=1U;
    CHECK(gs3d_layer_apply(&req,&mask,0)==GS3D_ERR_RANGE);
    mask.bone_count=2U;
    CHECK(gs3d_clip_create(&moving,7U,0U,0U,2U,100UL,0U,2U)==0);
    CHECK(gs3d_root_motion_delta(&moving,0UL,50UL,0U,&root)==0);
    CHECK(diff(root.position_q16[0],131072L)<5L);
    CHECK(diff(root.delta_q16[0],131072L)<5L);
    CHECK(gs3d_root_motion_delta(&moving,50UL,100UL,0U,&root)==0);
    CHECK(diff(root.position_q16[0],262144L)<5L);
    CHECK(diff(root.delta_q16[0],131072L)<5L);
    old=root;
    CHECK(gs3d_root_motion_delta(&moving,100UL,50UL,0U,&root)!=0);
    CHECK(root.delta_q16[0]==old.delta_q16[0]);
    moving.mode=GS3D_CLIP_LOOP;
    CHECK(gs3d_root_motion_delta(&moving,0UL,100UL,0U,&root)==GS3D_ERR_UNSUPPORTED);
    moving.mode=GS3D_CLIP_CLAMP;
    req.base=moving;req.base_ticks=50UL;req.previous_base_ticks=0UL;
    req.overlay=over;req.overlay_ticks=0UL;
    req.consume_root_motion=1U;req.root_bone=0U;
    CHECK(gs3d_layer_apply(&req,&mask,&root)==0);
    CHECK(diff(root.delta_q16[0],131072L)<5L);
    CHECK(diff(gs3d_model.v[1].x,pose.x)<250L);
    other=gs3d_model.v[1];
    CHECK(gs3d_layer_apply(&req,&mask,&root)==0);
    CHECK(gs3d_model.v[1].x==other.x && gs3d_model.v[1].y==other.y);
    req.consume_root_motion=0U;
    CHECK(gs3d_layer_apply(&req,&mask,&root)==0);
    CHECK(diff(gs3d_model.v[1].x-other.x,131072L)<250L);
    req.consume_root_motion=1U;
    CHECK(gs3d_mask_set(&mask,0U,GS3D_Q16_ONE)==0);
    CHECK(gs3d_layer_apply(&req,&mask,&root)==GS3D_ERR_UNSUPPORTED);
    CHECK(gs3d_mask_set(&mask,0U,0L)==0);
    CHECK(gs3d_layer_apply(&req,&mask,0)==GS3D_ERR_RANGE);
    req.root_bone=1U;
    CHECK(gs3d_layer_apply(&req,&mask,&root)==GS3D_ERR_RANGE);
    req.root_bone=0U;
    req.overlay_weight_q16=65537L;
    CHECK(gs3d_layer_apply(&req,&mask,&root)==GS3D_ERR_RANGE);
    req.overlay_weight_q16=65536L;
    for(gen=6U;gen<=8U;gen++){
        CHECK(gs3d_clip_create(&base,gen,0U,0U,1U,100UL,0U,2U)==0);
        CHECK(gs3d_clip_create(&over,gen,0U,1U,1U,100UL,0U,2U)==0);
        req.base=base;req.overlay=over;
        req.base_ticks=0UL;req.consume_root_motion=0U;
        CHECK(gs3d_vendor_layer_apply(gen,&req,&mask,0)==0);
        CHECK(gs3d_vendor_layer_apply(gen==8U?7U:8U,&req,&mask,0)==GS3D_ERR_UNSUPPORTED);
    }
    /* Deliberately corrupt a hierarchy in RAM: subtree must reject cycles. */
    gs3d_model.bone[0].parent=1U;
    r=gs3d_mask_subtree(&mask,1U,123L);
    CHECK(r==GS3D_ERR_CORRUPT);
    gs3d_model.bone[0].parent=65535U;
    gs3d_close();return 0;
}
static int multi_and_legacy(void){
    GS3D_LayerRequest req; GS3D_Clip c;GS3D_BoneMask mask;
    CHECK(gs3d_open("tests/fixtures/mdl7_two_weighted_groups.mdl")==0);
    CHECK(gs3d_mask_init(&mask,0L)==0);
    CHECK(gs3d_mask_subtree(&mask,1U,65536L)==0);
    CHECK(gs3d_clip_create(&c,7U,0U,0U,1U,100UL,0U,2U)==0);
    memset(&req,0,sizeof(req));req.base=req.overlay=c;
    req.overlay_weight_q16=65536L;
    CHECK(gs3d_layer_apply_all(&req,&mask,0)==0);
    CHECK(gs3d_model.groups==2U);
    gs3d_close();
    CHECK(gs3d_open("tests/fixtures/a4_mdl3.mdl")==0);
    CHECK(gs3d_mask_init(&mask,0L)==GS3D_ERR_UNSUPPORTED);
    CHECK(gs3d_vendor_layer_apply(4U,&req,&mask,0)!=0);
    CHECK(gs3d_vendor_layer_apply(5U,&req,&mask,0)!=0);
    gs3d_close();return 0;
}
int main(void){
    CHECK(skeleton()==0);
    CHECK(multi_and_legacy()==0);
    puts("PHASE10 PASS: masked skeleton, Q16 blending, root delta, root consumption, vendor routing");
    return 0;
}
