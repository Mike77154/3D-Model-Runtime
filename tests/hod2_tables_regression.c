/* HOD2 Phase12 H2TB native runtime regression, no model resources. */
#include "model_runtime89.h"
#include "modelrt_hod2_phase12.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char work[5UL*1024UL*1024UL];
static unsigned int observed;
static unsigned char bone_seen[128];
static int on_bone(void *ctx,const modelrt_bone_view *b)
{
 (void)ctx;
 if(b->index>=128u||bone_seen[b->index]||
    (b->parent!=-1&&(b->parent<0||b->parent>=128)))return 1;
 bone_seen[b->index]=1;observed++;return 0;
}
int main(int argc,char **argv)
{
 modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;
 modelrt_u32 profile=99u,parts;unsigned int i,checked=0u;
 int rc;
 if(argc!=3)return 2;
 modelrt_runtime_init(&rt);
 if(modelrt_register_builtin_providers(&rt)!=MODELRT_OK)return 3;
 modelrt_handle_init(&h,work,sizeof(work));
 rc=modelrt_open_file(&rt,&h,argv[1]);
 if(rc!=MODELRT_OK){fprintf(stderr,"open failed: %s\n",modelrt_last_error(&rt));return 4;}
 if(h.info.codec_id!=MODELRT_CODEC_HOD2||h.info.mesh_count||h.info.bone_count||
    h.info.animation_count||modelrt_hod2_table_profile(&h,&profile)!=MODELRT_OK||
    profile!=(modelrt_u32)strtoul(argv[2],0,10))return 5;
 memset(&sink,0,sizeof(sink));sink.on_bone=on_bone;
 for(i=0u;i<86u;i++){
  rc=modelrt_hod2_select_character(&h,(modelrt_u32)i);
  if(rc!=MODELRT_OK){fprintf(stderr,"selection %u returned %d\n",i,rc);return 6;}
  memset(bone_seen,0,sizeof(bone_seen));observed=0u;
  if(modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0)!=MODELRT_OK||
     !observed||observed!=h.info.bone_count)return 7;
  if(modelrt_hod2_extra_part_count(&h,(modelrt_u32)i,&parts)!=MODELRT_OK||parts>31u)return 8;
  checked++;
 }
 if(modelrt_hod2_select_character(&h,86u)!=MODELRT_ERR_BAD_ARG)return 9;
 if(modelrt_hod2_select_character(&h,15u)!=MODELRT_OK)return 10;
 if(modelrt_hod2_extra_part_count(&h,15u,&parts)!=MODELRT_OK)return 11;
 for(i=0u;i<parts;i++){
  modelrt_hod2_part_info part;
  rc=modelrt_hod2_extra_part_get(&h,15u,(modelrt_u32)i,&part);
  if(rc!=MODELRT_OK&&rc!=MODELRT_ERR_UNSUPPORTED)return 12;
 }
 printf("PASS Phase12 H2TB profile=%lu skeletons=%u type15 extra_parts=%lu\n",
        (unsigned long)profile,checked,(unsigned long)parts);
 modelrt_close(&h);
 return 0;
}
