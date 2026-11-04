#include "re4c3d/re4_coverage.h"
#include "re4c3d/re4_skeletal.h"
#include <string.h>

/* Coverage must use exactly the same *unique* FCV-to-skeleton mapping as
   the pose evaluator.  The old two-digit born__ parser silently mapped
   born__100 to born__10, and classified crow/bat clips as incompatible. */
re4_err re4_character_clip_coverage(const re4_pmd_model *m,
    const re4_fcv *fcv,re4_clip_coverage *out)
{
    re4_u32 i;
    re4_u8 seen[256];
    if(!m || !fcv || !out)return RE4_ERR_NULL;
    if((m->skeleton_name_count && !m->skeleton_names) ||
       (fcv->block_count && !fcv->blocks) ||
       (m->skeleton_count && !m->skeletons))return RE4_ERR_BAD_FORMAT;
    memset(out,0,sizeof(*out));memset(seen,0,sizeof(seen));
    out->skeleton_bones=m->skeleton_count;
    out->fcv_channels=fcv->block_count;
    for(i=0U;i<256U;++i){
        if(re4_skel_joint_for_fcv_id(m,(re4_u8)i)>=0)++out->name_mapped_bones;
    }
    for(i=0U;i<fcv->block_count;++i){
        const re4_fcv_block *b=&fcv->blocks[i];
        if(b->bone_type==2U){
            ++out->rotation_channels;
            if(re4_skel_joint_for_fcv_id(m,b->bone_no)>=0){
                ++out->linked_rotation_channels;
                if(!seen[b->bone_no]){
                    seen[b->bone_no]=1U;
                    ++out->unique_linked_bones;
                }
            }else ++out->unmatched_rotation_channels;
        }else ++out->special_channels;
    }
    return RE4_OK;
}
