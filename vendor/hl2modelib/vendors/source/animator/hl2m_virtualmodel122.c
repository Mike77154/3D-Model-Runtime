/* Source 2013 virtualmodel_t: opt-in, single included-model group.
   SDK reference: public/studio_virtualmodel.cpp AppendSequences,
   AppendAnimations, AppendBonemap, AppendPoseParameters.
   No allocation, IEEE floating point or 64-bit operations. */
#include "hl2modelib.h"
#include <string.h>
#include <limits.h>
#define V122_OVERRIDE 0x0800
#define V122_DESC_LIMIT HL2M_POSEPARAM_CAP121
static unsigned int v122_u32(const hl2m_u8 *d,unsigned int p)
{ return (unsigned int)d[p]|((unsigned int)d[p+1]<<8)|
    ((unsigned int)d[p+2]<<16)|((unsigned int)d[p+3]<<24); }
static int v122_span(unsigned int p,unsigned int n,unsigned int sz)
{ return p<=sz && n<=sz-p; }
static int v122_lower(int c)
{ return c>='A'&&c<='Z'?c+('a'-'A'):c; }
static int v122_equals(const char *a,const char *b,int path)
{
    unsigned int i=0u;int x,y;
    if(!a||!b)return 0;
    do{
        x=(unsigned char)a[i];y=(unsigned char)b[i];
        if(path){if(x=='\\')x='/';if(y=='\\')y='/';}
        if(v122_lower(x)!=v122_lower(y))return 0;
        if(!x)return 1;
        i++;
    }while(i<HL2M_PATH_MAX);
    return 0;
}
static int v122_read_relname(const hl2m_u8 *data,unsigned int size,
    unsigned int record,unsigned int field,char *name,unsigned int cap)
{
    unsigned int rel,p,j;int last=0;
    if(!name||!cap)return HL2M_ERR_NULL;
    name[0]=0;
    if(!v122_span(record+field,4u,size))return HL2M_ERR_RANGE;
    rel=v122_u32(data,record+field);
    if(rel==0u||rel>size-record)return HL2M_ERR_RANGE;
    p=record+rel;
    if(p>=size)return HL2M_ERR_RANGE;
    for(j=0u;j<cap;j++){
        if(!v122_span(p,j+1u,size))return HL2M_ERR_RANGE;
        last=(unsigned char)data[p+j];
        if(!last){name[j]=0;return HL2M_OK;}
        if(j==cap-1u)return HL2M_ERR_LIMIT;
        name[j]=(char)last;
    }
    return HL2M_ERR_LIMIT;
}
int hl2m_virtual_includes122(const hl2m_u8 *data,unsigned int size,
    char names[][HL2M_PATH_MAX],unsigned int capacity,unsigned int *count)
{
    unsigned int i,n,off,record;int r;
    if(!data||!count)return HL2M_ERR_NULL;
    if(!v122_span(0u,344u,size)||memcmp(data,"IDST",4)!=0)
        return HL2M_ERR_MAGIC;
    n=v122_u32(data,336u);off=v122_u32(data,340u);
    if(n>HL2M_VIRTUAL_INCLUDE_CAP122 || n>capacity)return HL2M_ERR_LIMIT;
    if(n&&(!names||!v122_span(off,n*8u,size)))return HL2M_ERR_RANGE;
    for(i=0u;i<n;i++){
        record=off+i*8u;
        r=v122_read_relname(data,size,record,4u,names[i],HL2M_PATH_MAX);
        if(r)return r;
    }
    *count=n;return HL2M_OK;
}
/* Name-based local bone mapping, as in AppendBonemap.  Parent conflicts are
   reported, not swept under the rug.  Unsupported hierarchies are rejected
   by the optional pose-remap API, not by the catalog parser. */
int hl2m_virtual_build122(const hl2m_u8 *base_bytes,unsigned int base_size,
    const hl2m_mdl *base,const char *include_path,
    const hl2m_u8 *child_bytes,unsigned int child_size,
    const hl2m_mdl *child,hl2m_virtual_link122 *out)
{
    char includes[HL2M_VIRTUAL_INCLUDE_CAP122][HL2M_PATH_MAX];
    hl2m_poseparam121 p0[V122_DESC_LIMIT],p1[V122_DESC_LIMIT];
    unsigned int inc_count=0u,n0=0u,n1=0u;
    int i,j,k,found=0,r,pa,pb;
    hl2m_virtual_link122 tmp;
    if(!base_bytes||!base||!include_path||!child_bytes||!child||!out)
        return HL2M_ERR_NULL;
    if(base->bone_count<0||base->bone_count>HL2M_MAX_BONES||
       child->bone_count<0||child->bone_count>HL2M_MAX_BONES||
       base->sequence_count<0||base->sequence_count>HL2M_MAX_MDL_SEQUENCES||
       child->sequence_count<0||child->sequence_count>HL2M_MAX_MDL_SEQUENCES||
       base->anim_count<0||base->anim_count>HL2M_MAX_MDL_ANIMS||
       child->anim_count<0||child->anim_count>HL2M_MAX_MDL_ANIMS)return HL2M_ERR_LIMIT;
    r=hl2m_virtual_includes122(base_bytes,base_size,includes,
        HL2M_VIRTUAL_INCLUDE_CAP122,&inc_count);if(r)return r;
    for(i=0;i<(int)inc_count;i++)if(v122_equals(includes[i],include_path,1))
        {found=1;break;}
    if(!found)return HL2M_ERR_RANGE;
    r=hl2m_poseparams_read121(base_bytes,base_size,p0,V122_DESC_LIMIT,&n0);
    if(r)return r;
    r=hl2m_poseparams_read121(child_bytes,child_size,p1,V122_DESC_LIMIT,&n1);
    if(r)return r;
    memset(&tmp,0,sizeof(tmp));
    tmp.base_bones=base->bone_count;tmp.child_bones=child->bone_count;
    for(i=0;i<HL2M_MAX_BONES;i++){
        tmp.child_to_base_bone[i]=-1;tmp.base_to_child_bone[i]=-1;
    }
    for(i=0;i<child->bone_count;i++){
        for(j=0;j<base->bone_count;j++)
            if(v122_equals(child->bones[i].name,base->bones[j].name,0))break;
        if(j==base->bone_count){tmp.unmapped_child_bones++;continue;}
        if(tmp.base_to_child_bone[j]>=0)return HL2M_ERR_PARSE;
        tmp.child_to_base_bone[i]=j;tmp.base_to_child_bone[j]=i;
        tmp.mapped_bones++;
    }
    for(i=0;i<child->bone_count;i++){
        j=tmp.child_to_base_bone[i];if(j<0)continue;
        pa=child->bones[i].parent;pb=base->bones[j].parent;
        if(pa< -1||pa>=child->bone_count||pb< -1||pb>=base->bone_count)
            return HL2M_ERR_PARSE;
        if(pa<0){if(pb>=0)tmp.parent_conflicts++;}
        else if(tmp.child_to_base_bone[pa]!=pb)tmp.parent_conflicts++;
    }
    for(i=0;i<base->sequence_count;i++){
        j=tmp.seq_count++;
        tmp.sequences[j].group=0;tmp.sequences[j].local=i;
    }
    for(i=0;i<child->sequence_count;i++){
        for(j=0;j<tmp.seq_count;j++){
            const hl2m_mdl_seqdesc *prior;
            const hl2m_virtual_ref122 *ref=&tmp.sequences[j];
            prior=ref->group?&child->sequences[ref->local]:&base->sequences[ref->local];
            if(v122_equals(prior->label,child->sequences[i].label,0))break;
        }
        if(j==tmp.seq_count){
            if(tmp.seq_count>=HL2M_VIRTUAL_SEQ_CAP122)return HL2M_ERR_LIMIT;
            k=tmp.seq_count++;tmp.sequences[k].group=1;
            tmp.sequences[k].local=i;
        }else{
            k=j;
            if((tmp.sequences[j].group?
                 child->sequences[tmp.sequences[j].local].flags:
                 base->sequences[tmp.sequences[j].local].flags) & V122_OVERRIDE){
                tmp.sequences[j].group=1;tmp.sequences[j].local=i;
            }
        }
        tmp.child_seq_to_virtual[i]=k;
    }
    for(i=0;i<base->anim_count;i++){
        j=tmp.anim_count++;
        tmp.animations[j].group=0;tmp.animations[j].local=i;
    }
    for(i=0;i<child->anim_count;i++){
        for(j=0;j<tmp.anim_count;j++){
            const hl2m_mdl_animdesc *prior;
            const hl2m_virtual_ref122 *ref=&tmp.animations[j];
            prior=ref->group?&child->anims[ref->local]:&base->anims[ref->local];
            if(v122_equals(prior->name,child->anims[i].name,0))break;
        }
        if(j==tmp.anim_count){
            if(tmp.anim_count>=HL2M_VIRTUAL_ANIM_CAP122)return HL2M_ERR_LIMIT;
            k=tmp.anim_count++;tmp.animations[k].group=1;
            tmp.animations[k].local=i;
        }else k=j;
        tmp.child_anim_to_virtual[i]=k;
    }
    for(i=0;i<(int)n0;i++){
        k=tmp.pose_count++;tmp.poses[k].group=0;tmp.poses[k].local=i;
        tmp.pose_names[k][0]=0;
        memcpy(tmp.pose_names[k],p0[i].name,HL2M_NAME_MAX);
        tmp.pose_start[k]=p0[i].start;tmp.pose_end[k]=p0[i].end;
        tmp.pose_loop[k]=p0[i].loop;
    }
    for(i=0;i<(int)n1;i++){
        for(j=0;j<tmp.pose_count;j++)
            if(v122_equals(tmp.pose_names[j],p1[i].name,0))break;
        if(j==tmp.pose_count){
            if(tmp.pose_count>=HL2M_VIRTUAL_POSE_CAP122)return HL2M_ERR_LIMIT;
            k=tmp.pose_count++;
            tmp.poses[k].group=1;tmp.poses[k].local=i;
            memcpy(tmp.pose_names[k],p1[i].name,HL2M_NAME_MAX);
            tmp.pose_start[k]=p1[i].start;tmp.pose_end[k]=p1[i].end;
            tmp.pose_loop[k]=p1[i].loop;
        }else{
            k=j;
            if(p1[i].start<tmp.pose_start[k])tmp.pose_start[k]=p1[i].start;
            if(p1[i].end>tmp.pose_end[k])tmp.pose_end[k]=p1[i].end;
        }
        tmp.child_pose_to_virtual[i]=k;
    }
    tmp.base_pose_count=(int)n0;tmp.child_pose_count=(int)n1;
    *out=tmp;
    return HL2M_OK;
}
/* Convert virtual normalized controllers back to the INCLUDED MDL's own
   normalized ranges.  Source merges named physical pose ranges. */
static int v122_convert_controls(const hl2m_virtual_link122 *link,
    int group,const hl2m_u8 *child_bytes,unsigned int child_size,
    const hl2m_fixed *virtual_norm,unsigned int count,
    hl2m_fixed *local_norm,unsigned int capacity)
{
    hl2m_poseparam121 child[HL2M_POSEPARAM_CAP121],range;
    unsigned int n=0u,i;int r,j;hl2m_fixed physical;
    if(!link||!child_bytes||!local_norm)return HL2M_ERR_NULL;
    r=hl2m_poseparams_read121(child_bytes,child_size,child,
        HL2M_POSEPARAM_CAP121,&n);if(r)return r;
    if(n!=(unsigned int)(group?link->child_pose_count:link->base_pose_count)||
       n>capacity)return HL2M_ERR_RANGE;
    if(count<(unsigned int)link->pose_count || (count&&!virtual_norm))
        return HL2M_ERR_RANGE;
    for(i=0u;i<n;i++){
        j=group?link->child_pose_to_virtual[i]:(int)i;
        if(j<0||j>=link->pose_count)return HL2M_ERR_RANGE;
        memset(&range,0,sizeof(range));
        range.start=link->pose_start[j];range.end=link->pose_end[j];
        range.loop=link->pose_loop[j];
        r=hl2m_pose_physical_from_control121(&range,virtual_norm[j],&physical);
        if(r)return r;
        r=hl2m_pose_control_from_physical121(&child[i],physical,&local_norm[i]);
        if(r)return r;
    }
    return HL2M_OK;
}
int hl2m_virtual_child_controls122(const hl2m_virtual_link122 *link,
    const hl2m_u8 *child_bytes,unsigned int child_size,
    const hl2m_fixed *virtual_norm,unsigned int count,
    hl2m_fixed *local_norm,unsigned int capacity)
{
    return v122_convert_controls(link,1,child_bytes,child_size,
        virtual_norm,count,local_norm,capacity);
}
int hl2m_virtual_base_controls122(const hl2m_virtual_link122 *link,
    const hl2m_u8 *base_bytes,unsigned int base_size,
    const hl2m_fixed *virtual_norm,unsigned int count,
    hl2m_fixed *local_norm,unsigned int capacity)
{
    return v122_convert_controls(link,0,base_bytes,base_size,
        virtual_norm,count,local_norm,capacity);
}
/* 32-bit-only bounded fixed-point matrix composition (no heap / int64). */
static int v122_mul(hl2m_fixed a,hl2m_fixed b,hl2m_fixed *res)
{
    unsigned int aa,bb,a_hi,b_hi,a_lo,b_lo,lo,high,cross,limit;
    int neg=(a<0)!=(b<0);
    aa=a<0?0u-(unsigned int)a:(unsigned int)a;
    bb=b<0?0u-(unsigned int)b:(unsigned int)b;
    a_hi=aa>>16;b_hi=bb>>16;a_lo=aa&65535u;b_lo=bb&65535u;
    limit=neg?2147483648u:2147483647u;
    /* The full products are bounded by the remaining capacity before sum. */
    if(a_hi&&b_hi>limit/(a_hi*65536u))return HL2M_ERR_RANGE;
    high=a_hi*b_hi*65536u;
    if(a_hi&&b_lo>(limit-high)/a_hi)return HL2M_ERR_RANGE;
    cross=a_hi*b_lo;high+=cross;
    if(b_hi&&a_lo>(limit-high)/b_hi)return HL2M_ERR_RANGE;
    high+=b_hi*a_lo;
    lo=(a_lo*b_lo)>>16;
    if(lo>limit-high)return HL2M_ERR_RANGE;
    high+=lo;
    if(neg){*res=high==2147483648u?INT_MIN:-(int)high;}
    else *res=(int)high;
    return HL2M_OK;
}
static int v122_add(hl2m_fixed a,hl2m_fixed b,hl2m_fixed *r)
{
    if((b>0&&a>INT_MAX-b)||(b<0&&a<INT_MIN-b))return HL2M_ERR_RANGE;
    *r=a+b;return HL2M_OK;
}
static int v122_mmul(const hl2m_mat3x4 *a,const hl2m_mat3x4 *b,
    hl2m_mat3x4 *out)
{
    hl2m_mat3x4 temp;hl2m_fixed mul,sum;int i,j,k,r;
    for(i=0;i<3;i++)for(j=0;j<4;j++){
        sum=0;
        for(k=0;k<3;k++){
            r=v122_mul(a->m[i][k],b->m[k][j],&mul);if(r)return r;
            r=v122_add(sum,mul,&sum);if(r)return r;
        }
        if(j==3){r=v122_add(sum,a->m[i][3],&sum);if(r)return r;}
        temp.m[i][j]=sum;
    }
    *out=temp;return HL2M_OK;
}
/* Map INCLUDED local transforms onto BASE bone names and rebuild BASE world
   hierarchy. This is not Valve's IK, world-space special layers or SRC bone
   pre/post-transform system; mismatching parent hierarchies fail explicitly. */
int hl2m_virtual_remap_pose122(const hl2m_mdl *base,
    const hl2m_virtual_link122 *link,const hl2m_pose_matrices *child_pose,
    hl2m_pose_matrices *out)
{
    hl2m_pose_matrices staging;int i,b,p,r;
    if(!base||!link||!child_pose||!out)return HL2M_ERR_NULL;
    if(base->bone_count!=link->base_bones||child_pose->bone_count!=link->child_bones)
        return HL2M_ERR_RANGE;
    if(link->parent_conflicts)return HL2M_ERR_UNSUPPORTED;
    r=hl2m_native_bind_matrices89(base,&staging);if(r)return r;
    for(i=0;i<link->child_bones;i++){
        b=link->child_to_base_bone[i];
        if(b>=0){
            if(b>=base->bone_count)return HL2M_ERR_RANGE;
            staging.local[b]=child_pose->local[i];
        }
    }
    for(i=0;i<staging.bone_count;i++){
        p=staging.parent[i];if(p>=i || p< -1)return HL2M_ERR_PARSE;
        if(p<0)staging.world[i]=staging.local[i];
        else{
            r=v122_mmul(&staging.world[p],&staging.local[i],&staging.world[i]);
            if(r)return r;
        }
    }
    *out=staging;return HL2M_OK;
}
