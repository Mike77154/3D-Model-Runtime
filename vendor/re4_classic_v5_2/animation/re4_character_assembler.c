/* RE4 2007 multipart PMD skeleton adapter. C89 / Q16.16 / no heap. */
#include "re4c3d/re4_character.h"
#include <string.h>
#define ONE 65536
#define MAXI 2147483647
#define MINI (-2147483647-1)
static re4_q16 add(re4_q16 a,re4_q16 b)
{
    if(b>0 && a>MAXI-b)return MAXI;
    if(b<0 && a<MINI-b)return MINI;
    return a+b;
}
static re4_q16 mul(re4_q16 a,re4_q16 b)
{
    re4_u32 ua,ub,h1,l1,h2,l2,r,p;int negative;
    negative=(a<0)!=(b<0);
    ua=(a<0)?(re4_u32)(-(a+1))+1U:(re4_u32)a;
    ub=(b<0)?(re4_u32)(-(b+1))+1U:(re4_u32)b;
    h1=ua>>16;l1=ua&65535U;h2=ub>>16;l2=ub&65535U;
    if(h1 && h2>32767U/h1)return negative?MINI:MAXI;
    r=(h1*h2)<<16;
    p=h1*l2;if(p>2147483647U-r)return negative?MINI:MAXI;r+=p;
    p=h2*l1;if(p>2147483647U-r)return negative?MINI:MAXI;r+=p;
    p=(l1*l2)>>16;if(p>2147483647U-r)return negative?MINI:MAXI;r+=p;
    return negative?-(re4_q16)r:(re4_q16)r;
}
static void matmul(const re4_skel_matrix *a,const re4_skel_matrix *b,re4_skel_matrix *o)
{
    re4_skel_matrix tmp;re4_u32 r,c,k;re4_q16 s;
    for(r=0;r<3;++r)for(c=0;c<4;++c){
        s=0;
        for(k=0;k<3;++k)s=add(s,mul(a->m[r*4+k],b->m[k*4+c]));
        if(c==3)s=add(s,a->m[r*4+3]);
        tmp.m[r*4+c]=s;
    }
    *o=tmp;
}
/* Bind skeletons that have been accepted by re4_skel_evaluate have
   identity bind quaternions, hence their world bases are rigid. */
static void rigid_inverse(const re4_skel_matrix *a,re4_skel_matrix *o)
{
    re4_u32 r,c,k;re4_q16 v;
    for(r=0;r<3;++r)for(c=0;c<3;++c)o->m[r*4+c]=a->m[c*4+r];
    for(r=0;r<3;++r){
        v=0;for(k=0;k<3;++k)v=add(v,mul(o->m[r*4+k],a->m[k*4+3]));
        o->m[r*4+3]=(v==MINI)?MAXI:-v;
    }
}
/* Decode complete skeleton identifiers. Earlier releases truncated born__100
   to born__10, which silently retargeted large-skeleton accessories onto
   incorrect joints. Accept a decimal identifier followed only by a known
   separator '_' (e.g. born__10_S16), or by end-of-name. */
static int get_id(re4_strref n)
{
    re4_size i,base;re4_u32 id,digits;re4_u8 ch;
    if(!n.ptr)return -1;
    if(n.len>=6U && !memcmp(n.ptr,"born__",6U))base=6U;
    /* Hand PMDs use born1__/born2__; map their numeric IDs onto born__.
       born3__ helper rigs deliberately remain unbound until audited. */
    else if(n.len>=7U && !memcmp(n.ptr,"born",4U) &&
            (n.ptr[4]=='1' || n.ptr[4]=='2') &&
            n.ptr[5]=='_' && n.ptr[6]=='_')base=7U;
    else if(n.len>=6U && (!memcmp(n.ptr,"lh00__",6U)||
                             !memcmp(n.ptr,"rh00__",6U)))base=6U;
    /* Ada's separately modeled hands name their two numeric joints LHAND__NN
       and RHAND__NN; they are bona fide named bone IDs, not mesh labels. */
    else if(n.len>=7U && (!memcmp(n.ptr,"LHAND__",7U)||
                              !memcmp(n.ptr,"RHAND__",7U)))base=7U;
    /* Observed Wesker pl0d hand PMD joint names. Do not accept any
       arbitrary family prefix: limit these aliases to confirmed forms. */
    else if(n.len>=10U && (!memcmp(n.ptr,"pl0dlh00__",10U)||
                               !memcmp(n.ptr,"pl0drh00__",10U)))base=10U;
    /* Krauser pl0a hands reuse em39-prefixed joint IDs. */
    else if(n.len>=10U && (!memcmp(n.ptr,"em39lh00__",10U)||
                               !memcmp(n.ptr,"em39rh00__",10U)))base=10U;
    /* Luis pl04 hand PMDs own special pl04lh04__/pl04rh01__ joints.
       Without aliases, the main born__ skeleton moves while these
       separate hand anchors remain in their T-pose (floating hands). */
    else if(n.len>=10U && (!memcmp(n.ptr,"pl04lh04__",10U)||
                               !memcmp(n.ptr,"pl04rh01__",10U)))base=10U;
    else return -1;
    id=0U;digits=0U;
    for(i=base;i<n.len;++i){
        ch=(re4_u8)n.ptr[i];
        if(ch<'0'||ch>'9')break;
        /* Keep identifiers bounded, so arbitrary malformed input cannot
           overflow a signed return value. */
        if(id>3276U || (id==3276U && (re4_u32)(ch-'0')>7U))return -1;
        id=id*10U+(re4_u32)(ch-'0');++digits;
    }
    if(digits<2U)return -1;
    if(i<n.len && n.ptr[i]!='_')return -1;
    return (int)id;
}
/* The crow body and its alternative wing meshes use exported joint names
   that are structurally equivalent only within the documented em23 family.
   Do not allow an arbitrary prefix-based numeric retarget. */
static int named_crow_joint(re4_strref n,int wing)
{
    const char *prefix;re4_size len,i;int id=0;
    if(!n.ptr)return -1;
    if(!wing){prefix="em2300_crow__";len=13U;}
    else if(n.len>=17U && !memcmp(n.ptr,"em2301_openWing__",17U))
         {prefix="em2301_openWing__";len=17U;}
    else {prefix="em2302_closeWing__";len=18U;}
    if(n.len<len+2U || memcmp(n.ptr,prefix,len)!=0)return -1;
    i=len;
    if(n.ptr[i]<'0'||n.ptr[i]>'9'||n.ptr[i+1U]<'0'||n.ptr[i+1U]>'9')return -1;
    id=(n.ptr[i]-'0')*10+n.ptr[i+1U]-'0';
    if(i+2U<n.len && n.ptr[i+2U]!='_')return -1;
    return id;
}
static re4_i32 crow_lookup(const re4_pmd_model *master,int id)
{
    re4_u32 i;re4_i32 found=-1;
    for(i=0;i<master->skeleton_count && i<master->skeleton_name_count;++i){
        if(named_crow_joint(master->skeleton_names[i].name,0)==id){
            if(found>=0)return -1;
            found=(re4_i32)i;
        }
    }
    return found;
}
/* Select only a main born skeleton, not LHAND/RHAND or helpers.
   Some original RE4 enemy models, notably em3c00, use born1__NN for
   their *master* rig. Only one source is permitted for a numeric joint:
   two born schemes containing the same ID fail closed. */
static re4_i32 lookup(const re4_pmd_model *master,int id)
{
    re4_u32 j;re4_i32 found=-1;re4_strref n;
    for(j=0;j<master->skeleton_count && j<master->skeleton_name_count;++j){
        n=master->skeleton_names[j].name;
        if(!n.ptr)continue;
        if(!((n.len>=6U && !memcmp(n.ptr,"born__",6U)) ||
             (n.len>=7U && (!memcmp(n.ptr,"born1__",7U) ||
                              !memcmp(n.ptr,"born2__",7U)))))continue;
        if(get_id(n)!=id)continue;
        if(found>=0)return -2; /* duplicate numeric identity: refuse link */
        found=(re4_i32)j;
    }
    return found;
}
re4_err re4_character_link(const re4_pmd_model *master,const re4_pmd_model *part,re4_character_links *l)
{
    re4_u32 i;int id;re4_i32 par;
    if(!master||!part||!l)return RE4_ERR_NULL;
    if(part->skeleton_count>RE4_SKEL_MAX_BONES||master->skeleton_count>RE4_SKEL_MAX_BONES)return RE4_ERR_NO_SPACE;
    memset(l,0,sizeof(*l));
    for(i=0;i<part->skeleton_count;++i){
        l->master_index[i]=-1;
        id=(i<part->skeleton_name_count)?get_id(part->skeleton_names[i].name):-1;
        if(id>=0){
            l->master_index[i]=lookup(master,id);
            if(l->master_index[i]==-2)return RE4_ERR_BAD_FORMAT;
            if(l->master_index[i]>=0){l->link_kind[i]=1U;++l->direct_links;}
        }
        else if(i<part->skeleton_name_count){
            /* em23 wing mesh may retarget only to an em23 body joint. */
            id=named_crow_joint(part->skeleton_names[i].name,1);
            if(id>=0){
                l->master_index[i]=crow_lookup(master,id);
                if(l->master_index[i]>=0){l->link_kind[i]=1U;++l->direct_links;}
            }
        }
        if(l->master_index[i]<0){
            par=part->skeletons[i].parent;
            /* Find nearest mapped ancestor, including across unnamed intermediary
               skeleton bones. A direct-parent-only lookup dropped accessories. */
            while(par>=0 && (re4_u32)par<i && l->master_index[par]<0)
                par=part->skeletons[par].parent;
            if(par>=0 && (re4_u32)par<i && l->master_index[par]>=0){
                l->master_index[i]=l->master_index[par];l->link_kind[i]=2U;++l->inherited_links;
            }else ++l->stationary_links;
        }
    }
    return RE4_OK;
}
re4_err re4_character_rigid_anchor(const re4_pmd_model *master,
    const re4_pmd_model *part,re4_u32 master_joint,
    re4_character_links *links)
{
 re4_u32 i;
 if(!master||!part||!links)return RE4_ERR_NULL;
 if(master_joint>=master->skeleton_count ||
    part->skeleton_count>RE4_SKEL_MAX_BONES ||
    master->skeleton_count>RE4_SKEL_MAX_BONES ||
    part->skeleton_count==0U)return RE4_ERR_RANGE;
 memset(links,0,sizeof(*links));
 for(i=0U;i<part->skeleton_count;++i){
    links->master_index[i]=(re4_i32)master_joint;
    links->link_kind[i]=2U; /* explicit shared motion, not a direct name match */
 }
 links->inherited_links=part->skeleton_count;
 return RE4_OK;
}
re4_err re4_character_retarget(const re4_pmd_model *master,
    const re4_skel_pose *master_rest,const re4_skel_pose *master_animated,
    const re4_pmd_model *part,const re4_skel_pose *part_rest,
    const re4_character_links *links,re4_skel_pose *out,re4_u32 capacity)
{
    re4_u32 i;re4_i32 j;re4_skel_matrix inv,delta;
    if(!master||!master_rest||!master_animated||!part||!part_rest||!links||!out||!out->world)return RE4_ERR_NULL;
    if(part->skeleton_count>RE4_SKEL_MAX_BONES||capacity<part->skeleton_count)return RE4_ERR_NO_SPACE;
    if(master_rest->bone_count<master->skeleton_count||master_animated->bone_count<master->skeleton_count||part_rest->bone_count<part->skeleton_count)return RE4_ERR_BAD_FORMAT;
    for(i=0;i<part->skeleton_count;++i){
        j=links->master_index[i];
        if(j<0){out->world[i]=part_rest->world[i];continue;}
        if((re4_u32)j>=master->skeleton_count)return RE4_ERR_BAD_FORMAT;
        rigid_inverse(&master_rest->world[j],&inv);
        matmul(&master_animated->world[j],&inv,&delta);
        matmul(&delta,&part_rest->world[i],&out->world[i]);
    }
    out->bone_count=part->skeleton_count;
    out->applied_rotation_channels=master_animated->applied_rotation_channels;
    out->ignored_special_channels=master_animated->ignored_special_channels;
    return RE4_OK;
}


/* Duplicate logical joint identifiers are normally an error. Luis pl04
   hand pieces contain both a whole born__ skeleton and a specialized
   pl04lh04__/pl04rh01__ helper. Only that *exact* born+helper pair may
   share a target; no two helpers, two born nodes or third alias may. */
static int luis_helper(re4_strref n)
{
    return n.ptr && n.len>=10U &&
         (!memcmp(n.ptr,"pl04lh04__",10U)||!memcmp(n.ptr,"pl04rh01__",10U));
}
static int ordinary_born(re4_strref n)
{
    return n.ptr && n.len>=6U && !memcmp(n.ptr,"born__",6U);
}
static int approved_duplicate(const re4_pmd_model *part,
    const re4_character_links *links,re4_u32 at,re4_i32 target)
{
    re4_u32 k,previous=0U,old=0U;
    re4_strref now,prior;
    if(at>=part->skeleton_name_count)return 0;
    now=part->skeleton_names[at].name;
    for(k=0U;k<at;++k){
        if(links->link_kind[k]==1U && links->master_index[k]==target){
            old=k;++previous;
        }
    }
    if(previous!=1U || old>=part->skeleton_name_count)return 0;
    prior=part->skeleton_names[old].name;
    return (luis_helper(now)&&ordinary_born(prior)) ||
           (ordinary_born(now)&&luis_helper(prior));
}

/* Examine risk patterns without attempting to choose the correct variant. */
re4_err re4_character_audit(const re4_pmd_model *master,
                           const re4_pmd_model *part,
                           const re4_character_links *links,
                           re4_character_link_audit *out)
{
    re4_u8 seen[RE4_SKEL_MAX_BONES];
    re4_u32 i,k,steps;re4_i32 target,ancestor,p,m;
    if(!master||!part||!links||!out)return RE4_ERR_NULL;
    if(!master->skeletons||!part->skeletons)return RE4_ERR_NULL;
    if(master->skeleton_count>RE4_SKEL_MAX_BONES||
       part->skeleton_count>RE4_SKEL_MAX_BONES)return RE4_ERR_NO_SPACE;
    memset(out,0,sizeof(*out));memset(seen,0,sizeof(seen));
    for(i=0U;i<part->skeleton_count;++i){
        target=links->master_index[i];
        if(links->link_kind[i]==0U){
            ++out->stationary;
            if(target!=-1)++out->out_of_range_targets;
            continue;
        }
        if(target<0||(re4_u32)target>=master->skeleton_count){
            ++out->out_of_range_targets;continue;
        }
        if(links->link_kind[i]==2U){++out->inherited;continue;}
        if(links->link_kind[i]!=1U){++out->out_of_range_targets;continue;}
        ++out->direct;
        if(seen[target]){
            if(approved_duplicate(part,links,i,target))
                ++out->recognized_hand_alias_pairs;
            else ++out->repeated_direct_targets;
        }else{seen[target]=1U;++out->unique_direct_targets;}
        /* Only compare a directly linked joint with the nearest directly
           linked ancestor; inherited mappings are never independent bones. */
        ancestor=-1;p=part->skeletons[i].parent;steps=0U;
        while(p>=0 && steps++<part->skeleton_count){
            if((re4_u32)p>=part->skeleton_count)break;
            if(links->link_kind[p]==1U){ancestor=links->master_index[p];break;}
            p=part->skeletons[p].parent;
        }
        if(ancestor>=0){
            m=target;steps=0U;
            while(m>=0 && steps++<master->skeleton_count){
                if(m==ancestor)break;
                if((re4_u32)m>=master->skeleton_count)break;
                m=master->skeletons[m].parent;
            }
            if(m!=ancestor)++out->ancestry_conflicts;
        }
    }
    k=out->direct+out->inherited+out->stationary;
    if(k!=part->skeleton_count)return RE4_ERR_BAD_FORMAT;
    return RE4_OK;
}

/* A hierarchy difference is reviewable (world-space bone deltas), whereas
   duplicate direct targets, nonexistent targets, or zero direct links are not.
   Explicit opt-in does NOT establish that the chosen accessory is canonical. */
re4_err re4_character_check_policy(const re4_character_link_audit *a,
                                   re4_u32 policy)
{
    if(!a)return RE4_ERR_NULL;
    if(policy!=RE4_PART_POLICY_STRICT &&
       policy!=RE4_PART_POLICY_REVIEW_ANCESTRY)return RE4_ERR_RANGE;
    if(!a->direct || a->out_of_range_targets || a->repeated_direct_targets)
        return RE4_ERR_BAD_FORMAT;
    if(a->ancestry_conflicts && policy==RE4_PART_POLICY_STRICT)
        return RE4_ERR_UNSUPPORTED;
    return RE4_OK;
}
