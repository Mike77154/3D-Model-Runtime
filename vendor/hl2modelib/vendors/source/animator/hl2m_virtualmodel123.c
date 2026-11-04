/* v1.0.23 Source virtualmodel_t multi-group catalog. Separate C89 vendor.
   References: Source SDK 2013 public/studio_virtualmodel.cpp:
     AppendModels, AppendSequences, AppendAnimations, AppendBonemap,
     AppendPoseParameters. Only directly declared includes; no disk lookup. */
#include "hl2modelib.h"
#include <string.h>
#define V123_OVERRIDE 0x0800
static int v123_lower(int c){return c>='A'&&c<='Z'?c+'a'-'A':c;}
static int v123_equal(const char *a,const char *b,int path)
{
    unsigned int n;int x,y;
    if(!a||!b)return 0;
    for(n=0u;n<HL2M_PATH_MAX;n++){
        x=(unsigned char)a[n];y=(unsigned char)b[n];
        if(path){if(x=='\\')x='/';if(y=='\\')y='/';}
        if(v123_lower(x)!=v123_lower(y))return 0;
        if(x==0)return 1;
    }
    return 0;
}
/* Each group only deduplicates against sequences present BEFORE that group
   is appended (Source's numCheck), not against its own sequence list. */
static int v123_append_sequences(hl2m_virtual_multi123 *c,int group)
{
    const hl2m_mdl *m=c->groups[group].source.mdl;
    int old=c->seq_count,i,j,k,prior_group,prior_local;
    const hl2m_mdl *prior;
    for(i=0;i<m->sequence_count;i++){
        for(j=0;j<old;j++){
            prior_group=c->sequences[j].group;
            prior_local=c->sequences[j].local;
            prior=c->groups[prior_group].source.mdl;
            if(v123_equal(m->sequences[i].label,
                prior->sequences[prior_local].label,0))break;
        }
        if(j==old){
            if(c->seq_count>=HL2M_VIRTUAL_SEQ_CAP123)return HL2M_ERR_LIMIT;
            k=c->seq_count++;
            c->sequences[k].group=group;c->sequences[k].local=i;
        }else{
            k=j;prior_group=c->sequences[j].group;
            prior_local=c->sequences[j].local;
            prior=c->groups[prior_group].source.mdl;
            if(prior->sequences[prior_local].flags & V123_OVERRIDE){
                c->sequences[j].group=group;
                c->sequences[j].local=i;
            }
        }
        c->groups[group].seq_to_virtual[i]=k;
    }
    return HL2M_OK;
}
static int v123_append_animations(hl2m_virtual_multi123 *c,int group)
{
    const hl2m_mdl *m=c->groups[group].source.mdl;
    int old=c->anim_count,i,j,k,pg,pl;
    const hl2m_mdl *prior;
    for(i=0;i<m->anim_count;i++){
        for(j=0;j<old;j++){
            pg=c->animations[j].group;pl=c->animations[j].local;
            prior=c->groups[pg].source.mdl;
            if(v123_equal(m->anims[i].name,prior->anims[pl].name,0))break;
        }
        if(j==old){
            if(c->anim_count>=HL2M_VIRTUAL_ANIM_CAP123)return HL2M_ERR_LIMIT;
            k=c->anim_count++;
            c->animations[k].group=group;c->animations[k].local=i;
        }else k=j;
        c->groups[group].anim_to_virtual[i]=k;
    }
    return HL2M_OK;
}
static int v123_append_poses(hl2m_virtual_multi123 *c,int group)
{
    hl2m_poseparam121 desc[HL2M_POSEPARAM_CAP121];
    const hl2m_virtual_input123 *s=&c->groups[group].source;
    unsigned int n=0u;int i,j,k,r,old;
    r=hl2m_poseparams_read121(s->mdl_bytes,s->mdl_size,desc,
        HL2M_POSEPARAM_CAP121,&n);if(r)return r;
    c->groups[group].pose_count=(int)n;
    old=c->pose_count;
    for(i=0;i<(int)n;i++){
        for(j=0;j<old;j++)
            if(v123_equal(c->pose_names[j],desc[i].name,0))break;
        if(j==old){
            if(c->pose_count>=HL2M_VIRTUAL_POSE_CAP123)return HL2M_ERR_LIMIT;
            k=c->pose_count++;
            memcpy(c->pose_names[k],desc[i].name,HL2M_NAME_MAX);
            c->pose_start[k]=desc[i].start;
            c->pose_end[k]=desc[i].end;
            c->pose_loop[k]=desc[i].loop;
        }else{
            k=j;
            if(desc[i].start<c->pose_start[k])c->pose_start[k]=desc[i].start;
            if(desc[i].end>c->pose_end[k])c->pose_end[k]=desc[i].end;
        }
        c->groups[group].pose_to_virtual[i]=k;
    }
    return HL2M_OK;
}
static int v123_build_common(const hl2m_virtual_input123 *inputs,
    unsigned int count,hl2m_virtual_multi123 *out,int require_direct)
{
    hl2m_virtual_multi123 *tmp;
    char includes[HL2M_VIRTUAL_INCLUDE_CAP122][HL2M_PATH_MAX];
    unsigned int inc_count=0u,i,j,k;int r,b,pa,pb,found;
    const hl2m_mdl *base,*child;
    /* Local stack scratch: reentrant, transactional until the final copy. */
    hl2m_virtual_multi123 work;
    if(!inputs||!out)return HL2M_ERR_NULL;
    if(count==0u||count>HL2M_VIRTUAL_GROUP_CAP123)return HL2M_ERR_LIMIT;
    if(!inputs[0].mdl||!inputs[0].mdl_bytes)return HL2M_ERR_NULL;
    base=inputs[0].mdl;
    if(base->bone_count<0||base->bone_count>HL2M_MAX_BONES)
        return HL2M_ERR_LIMIT;
    if(require_direct){
        r=hl2m_virtual_includes122(inputs[0].mdl_bytes,inputs[0].mdl_size,
            includes,HL2M_VIRTUAL_INCLUDE_CAP122,&inc_count);
        if(r)return r;
        /* Legacy v123 insists that all groups are direct base includes. */
        if(inc_count!=count-1u)return HL2M_ERR_RANGE;
    }
    for(i=1u;i<count;i++){
        if(!inputs[i].mdl||!inputs[i].mdl_bytes||!inputs[i].include_path)
            return HL2M_ERR_NULL;
        found=0;
        for(k=1u;k<i;k++)if(v123_equal(inputs[k].include_path,
            inputs[i].include_path,1))return HL2M_ERR_PARSE;
        if(require_direct){
            for(j=0u;j<inc_count;j++)if(v123_equal(
                inputs[i].include_path,includes[j],1)){
                found=1;break;
            }
            if(!found)return HL2M_ERR_RANGE;
        }
    }
    memset(&work,0,sizeof(work));tmp=&work;
    tmp->group_count=(int)count;
    for(i=0u;i<count;i++){
        hl2m_virtual_group123 *g=&tmp->groups[i];
        child=inputs[i].mdl;
        if(child->bone_count<0||child->bone_count>HL2M_MAX_BONES||
           child->sequence_count<0||
           child->sequence_count>HL2M_MAX_MDL_SEQUENCES||
           child->anim_count<0||child->anim_count>HL2M_MAX_MDL_ANIMS)
            return HL2M_ERR_LIMIT;
        g->source=inputs[i];
        for(j=0u;j<HL2M_MAX_BONES;j++){
            g->bone_to_base[j]=-1;g->base_to_bone[j]=-1;
        }
        for(j=0u;j<(unsigned int)child->bone_count;j++){
            if(i==0u){g->bone_to_base[j]=(int)j;
                g->base_to_bone[j]=(int)j;g->mapped_bones++;continue;}
            for(k=0u;k<(unsigned int)base->bone_count;k++)
                if(v123_equal(child->bones[j].name,base->bones[k].name,0))break;
            if(k==(unsigned int)base->bone_count){g->unmapped_bones++;continue;}
            if(g->base_to_bone[k]>=0)return HL2M_ERR_PARSE;
            g->bone_to_base[j]=(int)k;g->base_to_bone[k]=(int)j;
            g->mapped_bones++;
        }
        if(i!=0u){
            for(j=0u;j<(unsigned int)child->bone_count;j++){
                b=g->bone_to_base[j];if(b<0)continue;
                pa=child->bones[j].parent;pb=base->bones[b].parent;
                if(pa< -1||pa>=child->bone_count||pb< -1||pb>=base->bone_count)
                    return HL2M_ERR_PARSE;
                if(pa<0){if(pb>=0)g->parent_conflicts++;}
                else if(g->bone_to_base[pa]!=pb)g->parent_conflicts++;
            }
        }
        r=v123_append_sequences(tmp,(int)i);if(r)return r;
        r=v123_append_animations(tmp,(int)i);if(r)return r;
        r=v123_append_poses(tmp,(int)i);if(r)return r;
    }
    *out=*tmp;return HL2M_OK;
}
/* Internal entry for v124: dependency graph has already been validated and
   ordered by the recursive resolver.  Keep v123's public validation intact. */
int hl2m_virtual_build_ordered124_internal(const hl2m_virtual_input123 *inputs,
    unsigned int count,hl2m_virtual_multi123 *out)
{ return v123_build_common(inputs,count,out,0); }
int hl2m_virtual_build_multi123(const hl2m_virtual_input123 *inputs,
    unsigned int count,hl2m_virtual_multi123 *out)
{ return v123_build_common(inputs,count,out,1); }

int hl2m_virtual_controls_multi123(const hl2m_virtual_multi123 *c,
    int group,const hl2m_fixed *global,unsigned int global_count,
    hl2m_fixed *local,unsigned int cap)
{
    hl2m_poseparam121 desc[HL2M_POSEPARAM_CAP121],merged;
    const hl2m_virtual_group123 *g;unsigned int n=0u,i;
    int k,r;hl2m_fixed physical;
    if(!c||!local)return HL2M_ERR_NULL;
    if(group<0||group>=c->group_count)return HL2M_ERR_RANGE;
    g=&c->groups[group];
    if(global_count<(unsigned int)c->pose_count ||
       (global_count&&(!global)) || cap<(unsigned int)g->pose_count)
       return HL2M_ERR_RANGE;
    r=hl2m_poseparams_read121(g->source.mdl_bytes,g->source.mdl_size,
        desc,HL2M_POSEPARAM_CAP121,&n);if(r)return r;
    if(n!=(unsigned int)g->pose_count)return HL2M_ERR_RANGE;
    for(i=0u;i<n;i++){
        k=g->pose_to_virtual[i];if(k<0||k>=c->pose_count)return HL2M_ERR_RANGE;
        memset(&merged,0,sizeof(merged));
        merged.start=c->pose_start[k];merged.end=c->pose_end[k];
        merged.loop=c->pose_loop[k];
        r=hl2m_pose_physical_from_control121(&merged,global[k],&physical);
        if(r)return r;
        r=hl2m_pose_control_from_physical121(&desc[i],physical,&local[i]);
        if(r)return r;
    }
    return HL2M_OK;
}
int hl2m_virtual_remap_multi123(const hl2m_virtual_multi123 *c,
    int group,const hl2m_pose_matrices *pose,hl2m_pose_matrices *out)
{
    hl2m_virtual_link122 link;const hl2m_virtual_group123 *g;
    int i;
    if(!c||!pose||!out)return HL2M_ERR_NULL;
    if(group<0||group>=c->group_count)return HL2M_ERR_RANGE;
    if(group==0){
        if(pose->bone_count!=c->groups[0].source.mdl->bone_count)
            return HL2M_ERR_RANGE;
        *out=*pose;return HL2M_OK;
    }
    g=&c->groups[group];
    memset(&link,0,sizeof(link));
    link.base_bones=c->groups[0].source.mdl->bone_count;
    link.child_bones=g->source.mdl->bone_count;
    link.parent_conflicts=g->parent_conflicts;
    for(i=0;i<HL2M_MAX_BONES;i++){
        link.child_to_base_bone[i]=g->bone_to_base[i];
        link.base_to_child_bone[i]=g->base_to_bone[i];
    }
    return hl2m_virtual_remap_pose122(c->groups[0].source.mdl,
        &link,pose,out);
}
