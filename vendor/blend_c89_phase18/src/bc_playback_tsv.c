/* Basic armature playback from C89 fixed-point samples. Supports pose.bones
   channels: location, rotation_quaternion, scale. No IK, NLA, constraints,
   Euler, modifiers or Mesh/Object-space corrections. Unsupported data fails.
   Parent transforms use rest-local basis and rest inverse, not a flat pose. */
#include "bc.h"
#include "bc_rig_q16.h"
#include "bc_ik2.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#define BC_PLAY_BONES 8192U
#define BC_PLAY_LINE 2048U
#define BC_PLAY_TOKENS 40U

typedef struct BC_PlayBone {
  char name[96];
  int parent,length,rest_head[3],rest_tail[3];
  BC_RigMat rest,invrest,pose,deform;
  int loc[3],quat[4],scale[3],eul[3],rotmode;
  unsigned int locmask,quatmask,scalemask,eulmask;
  unsigned char has_pose_info;
} BC_PlayBone;
static BC_PlayBone bc_play_bones[BC_PLAY_BONES];
static BC_RigMat bc_play_palette[BC_PLAY_BONES];
static char bc_play_line[BC_PLAY_LINE];
static char *bc_play_tok[BC_PLAY_TOKENS];

static int bc_play_next(FILE *f,unsigned int *n)
{
  unsigned int count,bytes;
  char *p;
  for(;;){
    if(!fgets(bc_play_line,sizeof(bc_play_line),f))return ferror(f)?-1:1;
    bytes=(unsigned int)strlen(bc_play_line);
    if(bytes==BC_PLAY_LINE-1U && bc_play_line[bytes-1]!='\n' && !feof(f))return -1;
    p=bc_play_line; count=0U;
    /* Blender-derived TSV may carry spaces in bone names and RNA paths.
       Prefer TAB separation whenever a line has tabs; accept whitespace-only
       legacy interchange fixtures as a compatibility fallback. */
    {
      int tabmode=(strchr(p,'\t')!=NULL);
      while(*p==' '||*p=='\t')p++;
      if(*p=='#'||*p=='\r'||*p=='\n'||!*p)continue;
      for(;;){
        if(count>=BC_PLAY_TOKENS)return -1;
        bc_play_tok[count++]=p;
        if(tabmode){
          while(*p &&*p!='\t'&&*p!='\r'&&*p!='\n')p++;
        }else{
          while(*p &&*p!=' '&&*p!='\t'&&*p!='\r'&&*p!='\n')p++;
        }
        if(!*p)break;
        if(*p=='\r'||*p=='\n'){*p=0;break;}
        *p=0;p++;
        if(!tabmode)while(*p==' '||*p=='\t')p++;
        if(!*p||*p=='\r'||*p=='\n')break;
      }
    }
    if(count){*n=count;return 0;}
  }
}
static int bc_play_uint(const char *s,unsigned int *v)
{
  unsigned int out=0U,d;
  if(!*s)return -1;
  while(*s){
    if(*s<'0'||*s>'9')return -1;
    d=(unsigned int)(*s-'0');
    if(out>(UINT_MAX-d)/10U)return -1;
    out=out*10U+d;s++;
  }
  *v=out;return 0;
}
static int bc_play_int(const char *s,int *v)
{
  unsigned int x;
  if(*s=='-'){
    if(bc_play_uint(s+1,&x)!=0 || x>2147483648U)return -1;
    *v=x==2147483648U?INT_MIN:-(int)x;
  }else{
    if(bc_play_uint(s,&x)!=0 ||x>2147483647U)return -1;
    *v=(int)x;
  }
  return 0;
}
static int bc_play_rest_select(FILE *in,unsigned int *numbones,unsigned int arm_index)
{
  unsigned int n,id,arm,k,i,j;int parent,v,r;
  BC_PlayBone *bone;
  *numbones=0U;
  for(;;){
    r=bc_play_next(in,&n);
    if(r==1)break;
    if(r!=0 ||n!=20U ||bc_play_uint(bc_play_tok[0],&arm)!=0)return -1;
    if(arm!=arm_index)continue;
    if(bc_play_uint(bc_play_tok[1],&id)!=0 ||id!=*numbones ||id>=BC_PLAY_BONES ||
       bc_play_int(bc_play_tok[2],&parent)!=0 ||
       (parent>=0 &&(unsigned int)parent>=id) ||parent < -1 ||
       strlen(bc_play_tok[3])>=sizeof(bc_play_bones[0].name))return -1;
    bone=&bc_play_bones[id];memset(bone,0,sizeof(*bone));
    strcpy(bone->name,bc_play_tok[3]);bone->parent=parent;
    bc_rig_mat_identity(&bone->rest);
    for(k=0U;k<16U;k++){
      if(bc_parse_q16(bc_play_tok[k+4U],&v)!=BC_OK)return -1;
      bone->rest.m[k]=v;
    }
    if(bc_rig_inverse_rigid(&bone->rest,&bone->invrest)!=BC_RIG_OK)return -1;
    bone->quat[0]=BC_RIG_ONE;
    for(i=0U;i<3U;i++)bone->scale[i]=BC_RIG_ONE;
    for(j=0U;j<id;j++)if(strcmp(bone->name,bc_play_bones[j].name)==0)return -1;
    ++(*numbones);
  }
  return *numbones?0:-1;
}
static int bc_play_rest(FILE *in,unsigned int *numbones)
{return bc_play_rest_select(in,numbones,0U);}
/* Apply saved Euler rotation mode and base Euler components when an Object
   actually contains bPoseChannel records. A missing pose object is acceptable
   for synthetic files and direct action sampling; ambiguous duplicates fail. */
static int bc_play_pose_metadata_select(BC_File *blend,unsigned int bones,int object_id)
{
  FILE *pose=tmpfile();
  unsigned int n,i,j;
  int mode,r;
  if(!pose)return -1;
  r=bc_export_pose(blend,pose);
  if(r==BC_UNSUPPORTED){fclose(pose);return 0;}
  if(r!=BC_OK || fflush(pose)!=0 || fseek(pose,0,SEEK_SET)!=0){fclose(pose);return -1;}
  for(;;){
    r=bc_play_next(pose,&n);
    if(r==1)break;
    if(r!=0 ||n!=17U ||bc_play_int(bc_play_tok[16],&mode)!=0 ||
       mode< -1 ||mode>6){fclose(pose);return -1;}
    if(object_id>=0){
      unsigned int owner;
      if(bc_play_uint(bc_play_tok[0],&owner)!=0){fclose(pose);return -1;}
      if(owner!=(unsigned int)object_id)continue;
    }
    for(i=0U;i<bones;i++)if(strcmp(bc_play_bones[i].name,bc_play_tok[2])==0)break;
    if(i==bones)continue;
    if(bc_play_bones[i].has_pose_info){fclose(pose);return -1;}
    bc_play_bones[i].rotmode=mode;
    for(j=0U;j<3U;j++)if(bc_parse_q16(bc_play_tok[13U+j],&bc_play_bones[i].eul[j])!=BC_OK){fclose(pose);return -1;}
    bc_play_bones[i].has_pose_info=1U;
  }
  fclose(pose);
  return 0;
}
static int bc_play_pose_metadata(BC_File *blend,unsigned int bones)
{return bc_play_pose_metadata_select(blend,bones,-1);}
static int bc_play_channel(const char *s,unsigned int *bone,
                           unsigned int *which,unsigned int numbones)
{
  const char *start="pose.bones[\"";
  const char *tail;unsigned int i,len=0U;
  char name[96];
  if(strncmp(s,start,strlen(start))!=0)return 1;
  tail=s+strlen(start);
  while(*tail && *tail!='\"'){
    if(len+1U>=sizeof(name))return -1;
    if(*tail=='\\'){
      tail++;
      if(*tail!='\\' &&*tail!='\"')return -1;
      if(!*tail)return -1;
    }
    name[len++]=*tail++;
  }
  if(!*tail ||tail[1]!=']'||tail[2]!='.')return -1;
  name[len]=0;
  for(i=0U;i<numbones;i++){
    if(strcmp(bc_play_bones[i].name,name)==0){*bone=i;break;}
  }
  if(i==numbones)return -1;
  tail+=3U;
  if(strcmp(tail,"location")==0)*which=0U;
  else if(strcmp(tail,"rotation_quaternion")==0)*which=1U;
  else if(strcmp(tail,"scale")==0)*which=2U;
  else if(strcmp(tail,"rotation_euler")==0)*which=3U;
  else return -1;
  return 0;
}
static int bc_play_samples(FILE *samples,unsigned int numbones,unsigned int action,
                           unsigned int slot,int filter_slot)
{
  unsigned int n,id=0U,which=0U,comp,act,seen=0U,bit,*mask;
  int value,r,*channel;
  BC_PlayBone *bone;
  for(;;){
    r=bc_play_next(samples,&n);
    if(r==1)break;
    if(r!=0 ||n!=9U ||bc_play_uint(bc_play_tok[0],&act)!=0)return -1;
    if(act!=action)continue;
    if(filter_slot){
      unsigned int row_slot;
      if(bc_play_uint(bc_play_tok[3],&row_slot)!=0)return -1;
      if(row_slot!=slot)continue;
    }
    r=bc_play_channel(bc_play_tok[5],&id,&which,numbones);
    if(r==1)continue; /* other properties are not armature channels */
    if(r!=0 ||bc_play_uint(bc_play_tok[6],&comp)!=0 ||
       bc_parse_q16(bc_play_tok[8],&value)!=BC_OK)return -1;
    bone=&bc_play_bones[id];
    if(which==0U){if(comp>2U)return -1;channel=bone->loc;mask=&bone->locmask;}
    else if(which==1U){if(comp>3U)return -1;channel=bone->quat;mask=&bone->quatmask;}
    else if(which==2U){if(comp>2U)return -1;channel=bone->scale;mask=&bone->scalemask;}
    else {if(comp>2U)return -1;channel=bone->eul;mask=&bone->eulmask;}
    if(which==3U){
      if((bone->has_pose_info && bone->rotmode<=0) ||bone->quatmask)return -1;
      if(!bone->has_pose_info)bone->rotmode=1; /* XYZ fallback without serialized pose */
    }
    if(which==1U && ((bone->has_pose_info &&bone->rotmode!=0) ||bone->eulmask))return -1;
    bit=1U<<comp;
    if((*mask&bit)!=0U)return -1; /* ambiguity across slots/layers/strips */
    *mask|=bit;channel[comp]=value;seen++;
  }
  return seen?0:-1;
}
static int bc_play_normalize(int quat[4])
{
  int norm=0,term,x,quot,tmp;
  unsigned int i,j;
  for(i=0U;i<4U;i++){
    if(bc_qmul_checked(quat[i],quat[i],&term)!=0 ||
       bc_qadd_checked(norm,term,&norm)!=0)return -1;
  }
  if(norm<256 ||norm>4*BC_RIG_ONE)return -1;
  x=norm>BC_RIG_ONE?norm:BC_RIG_ONE;
  for(j=0U;j<14U;j++){
    if(bc_qdiv_checked(norm,x,&quot)!=0 ||
       bc_qadd_checked(x,quot,&tmp)!=0)return -1;
    x=tmp/2;
    if(x==0)return -1;
  }
  for(i=0U;i<4U;i++){
    if(bc_qdiv_checked(quat[i],x,&tmp)!=0)return -1;
    quat[i]=tmp;
  }
  return 0;
}
static int bc_play_constraints_setup(BC_File *b,unsigned int at,unsigned int bones,
                                     const BC_RigMat *arm_local)
{
  static const char *names[BC_PLAY_BONES];
  unsigned int i;
  for(i=0U;i<bones;i++)names[i]=bc_play_bones[i].name;
  return bc_constraints_prepare(b,at,names,bones,arm_local);
}
/* Read authentic Bone arm_head/arm_tail in the same stable BFS order as
   bc_export_rest_matrices. The rest frame may be inclined and rolled. */
static int bc_play_ik_lengths(BC_File *blend,unsigned int arm,unsigned int bones)
{
  FILE *f=tmpfile();unsigned int n,ai,id,k,seen=0U;int head[3],tail[3],r,len;
  if(!f)return -1;
  if(bc_export_bones(blend,f)!=BC_OK ||fflush(f)!=0 ||fseek(f,0,SEEK_SET)!=0){fclose(f);return -1;}
  for(;;){
    r=bc_play_next(f,&n);if(r==1)break;
    if(r!=0 ||n!=10U ||bc_play_uint(bc_play_tok[0],&ai)!=0 ||
       bc_play_uint(bc_play_tok[1],&id)!=0){fclose(f);return -1;}
    if(ai!=arm)continue;
    if(id!=seen ||id>=bones ||strcmp(bc_play_bones[id].name,bc_play_tok[3])!=0){fclose(f);return -1;}
    for(k=0U;k<3U;k++)if(bc_parse_q16(bc_play_tok[4U+k],&head[k])!=BC_OK ||
                          bc_parse_q16(bc_play_tok[7U+k],&tail[k])!=BC_OK){fclose(f);return -1;}
    if(bc_ik2_segment_length(head,tail,&len)==BC_RIG_OK &&len>256){
      BC_PlayBone *b=&bc_play_bones[id];
      for(k=0U;k<3U;k++){
        b->rest_head[k]=head[k];b->rest_tail[k]=tail[k];
      }
      b->length=len;
    }
    seen++;
  }
  fclose(f);return r==1 &&seen==bones?0:-1;
}
static int bc_play_ik_rotation_near(int a,int b)
{
  int difference;
  if(b==INT_MIN ||bc_qadd_checked(a,-b,&difference)!=BC_RIG_OK)return 0;
  return difference>=-192 &&difference<=192;
}
static int bc_play_ik_eval(unsigned int tip)
{
  BC_IK2Goal goal;BC_IK2Result result;
  BC_PlayBone *child=&bc_play_bones[tip],*root;
  BC_RigMat rest[2];int head[2][3],tail[2][3];
  int idx=child->parent,k;
  if(idx<0 ||(unsigned int)idx>=tip)return -1;
  root=&bc_play_bones[idx];
  if(root->parent!=-1 ||root->length<=256 ||child->length<=256 ||
     bc_constraints_target((unsigned int)idx)>=0 ||
     root->quatmask||root->eulmask||root->scalemask ||
     child->locmask||child->quatmask||child->eulmask||child->scalemask)return -1;
  if(bc_constraints_ik_goal(tip,&goal)!=1)return -1;
  /* A translated root pose is permitted; no root rotation or scale yet. */
  for(k=0;k<3;k++)goal.root[k]=root->pose.m[k*4+3];
  for(k=0;k<3;k++){
    int j;
    for(j=0;j<3;j++){
      if(!bc_play_ik_rotation_near(root->pose.m[k*4+j],root->rest.m[k*4+j]) ||
         !bc_play_ik_rotation_near(child->pose.m[k*4+j],child->rest.m[k*4+j]))return -1;
    }
  }
  goal.length[0]=root->length;goal.length[1]=child->length;
  rest[0]=root->rest;rest[1]=child->rest;
  for(k=0;k<3;k++){
    head[0][k]=root->rest_head[k];tail[0][k]=root->rest_tail[k];
    head[1][k]=child->rest_head[k];tail[1][k]=child->rest_tail[k];
  }
  if(bc_ik2_solve_rest(&goal,rest,&head[0][0],&tail[0][0],&result)!=BC_RIG_OK)return -1;
  root->pose=result.pose[0];child->pose=result.pose[1];
  if(bc_rig_mat_mul(&root->pose,&root->invrest,&root->deform)!=BC_RIG_OK)return -1;
  bc_play_palette[idx]=root->deform;
  return 0;
}
static int bc_play_pose(unsigned int num)
{
  unsigned int i;
  BC_PlayBone *b;
  BC_RigMat local,relative,mid;
  for(i=0U;i<num;i++){
    b=&bc_play_bones[i];
    if(b->rotmode>=1 && b->rotmode<=6){
      if(bc_rig_euler_quat(b->eul,(unsigned int)b->rotmode,b->quat)!=BC_RIG_OK)return -1;
    }else if(b->rotmode!=0)return -1; /* axis-angle not implemented */
    if(bc_play_normalize(b->quat)!=0 ||
       bc_rig_mat_from_trs(b->loc,b->quat,b->scale,&local)!=BC_RIG_OK)return -1;
    if(b->parent>=0){
      if(bc_rig_mat_mul(&bc_play_bones[b->parent].invrest,&b->rest,&relative)!=0 ||
         bc_rig_mat_mul(&bc_play_bones[b->parent].pose,&relative,&mid)!=0 ||
         bc_rig_mat_mul(&mid,&local,&b->pose)!=0)return -1;
    }else{
      if(bc_rig_mat_mul(&b->rest,&local,&b->pose)!=0)return -1;
    }
    {
      int target=bc_constraints_target(i);
      const BC_RigMat *target_pose=NULL;
      if(target>=0){
        if((unsigned int)target>=i)return -1;
        target_pose=&bc_play_bones[target].pose;
      }
      if(bc_constraints_apply(i,target_pose,&b->pose)!=BC_OK)return -1;
    }
    {
      BC_IK2Goal candidate;int isik=bc_constraints_ik_goal(i,&candidate);
      if(isik<0 ||(isik>0 &&bc_play_ik_eval(i)!=0))return -1;
    }
    if(bc_rig_mat_mul(&b->pose,&b->invrest,&b->deform)!=0)return -1;
    bc_play_palette[i]=b->deform;
  }
  return 0;
}
static int bc_play_vertices(FILE *verts,FILE *out,unsigned int num)
{
  unsigned int n,id,nb,i,idx;int p[3],dst[3],weight,r;
  BC_RigInfluence inf[BC_RIG_MAX_WEIGHTS];
  if(fputs("# vertex_index x y z (fixed16 decimal)\n",out)==EOF)return -1;
  for(;;){
    r=bc_play_next(verts,&n);
    if(r==1)break;
    if(r!=0||n<5U ||bc_play_uint(bc_play_tok[0],&id)!=0 ||
       bc_parse_q16(bc_play_tok[1],&p[0])!=0 ||
       bc_parse_q16(bc_play_tok[2],&p[1])!=0 ||
       bc_parse_q16(bc_play_tok[3],&p[2])!=0 ||
       bc_play_uint(bc_play_tok[4],&nb)!=0 ||
       nb>BC_RIG_MAX_WEIGHTS ||n!=5U+nb*2U)return -1;
    for(i=0U;i<nb;i++){
      if(bc_play_uint(bc_play_tok[5U+2U*i],&idx)!=0 ||idx>=num ||
         bc_parse_q16(bc_play_tok[6U+2U*i],&weight)!=BC_OK ||
         weight<0 ||weight>BC_RIG_ONE)return -1;
      inf[i].bone=idx;inf[i].weight=weight;
    }
    if(bc_rig_skin_point(bc_play_palette,num,p,inf,nb,dst)!=BC_RIG_OK)return -1;
    if(fprintf(out,"%u\t",id)<0)return -1;
    for(i=0U;i<3U;i++){
      if(i>0U && fputc('\t',out)==EOF)return -1;
      bc_print_q16(out,dst[i]);
    }
    if(fputc('\n',out)==EOF)return -1;
  }
  return ferror(out)?-1:0;
}
/* Direct playback of a supported armature from .blend, one requested frame.
   Vertex influences are supplied separately (already mapped to bone IDs). */
int bc_play_blend_frame(BC_File *blend,int frame,
                        const char *vertex_path,const char *output_path,
                        unsigned int action)
{
  FILE *rest=NULL,*samples=NULL,*verts=NULL,*out=NULL;
  unsigned int count=0U;
  int result=BC_FAILURE,created=0;
  if(strcmp(vertex_path,output_path)==0)return BC_FAILURE;
  if(bc_constraints_guard_global(blend)!=BC_OK)goto done;
  rest=tmpfile();if(!rest)goto done;
  samples=tmpfile();if(!samples)goto done;
  verts=fopen(vertex_path,"rb");if(!verts)goto done;
  if(bc_export_rest_matrices(blend,rest)!=BC_OK ||
     bc_export_sample(blend,samples,frame)!=BC_OK ||
     fflush(rest)!=0 ||fflush(samples)!=0 ||
     fseek(rest,0,SEEK_SET)!=0 ||fseek(samples,0,SEEK_SET)!=0 ||
     bc_play_rest(rest,&count)!=0 ||
     bc_play_pose_metadata(blend,count)!=0 ||
     bc_play_samples(samples,count,action,0U,0)!=0 ||
     bc_play_pose(count)!=0)goto done;
  out=fopen(output_path,"wb");if(!out)goto done;
  created=1;
  if(bc_play_vertices(verts,out,count)!=0)goto done;
  result=BC_OK;
done:
  if(out && fclose(out)!=0)result=BC_FAILURE;
  if(verts)fclose(verts);
  if(samples)fclose(samples);
  if(rest)fclose(rest);
  if(result!=BC_OK && created)remove(output_path);
  return result;
}

int bc_playback_tsv(const char *rest_path,const char *sample_path,
                    const char *vertices_path,const char *output_path,
                    unsigned int action)
{
  FILE *rest=NULL,*samples=NULL,*vertices=NULL,*out=NULL;
  unsigned int count=0U;
  int ok=-1,created=0;
  if(!strcmp(output_path,rest_path)||!strcmp(output_path,sample_path)||
     !strcmp(output_path,vertices_path))return BC_FAILURE;
  rest=fopen(rest_path,"rb");if(!rest)goto finish;
  samples=fopen(sample_path,"rb");if(!samples)goto finish;
  vertices=fopen(vertices_path,"rb");if(!vertices)goto finish;
  if(bc_play_rest(rest,&count)!=0 ||
     bc_play_samples(samples,count,action,0U,0)!=0 ||
     bc_play_pose(count)!=0)goto finish;
  out=fopen(output_path,"wb");if(!out)goto finish;
  created=1;
  if(bc_play_vertices(vertices,out,count)!=0)goto finish;
  ok=0;
finish:
  if(out && fclose(out)!=0)ok=-1;
  if(rest)fclose(rest);
  if(samples)fclose(samples);
  if(vertices)fclose(vertices);
  if(ok!=0&&created)remove(output_path);
  return ok==0?BC_OK:BC_FAILURE;
}

/* Auto-map mesh vertex-group NAMES to bone names, then emit an animated OBJ.
   Static bounded scratch space; this is an offline deform preview, not an NLA
   or full Blender evaluated depsgraph. */
#define BC_PLAY_MAX_VERTS 250000U
#define BC_PLAY_OBJ_LINE 262144U
typedef struct BC_PlayWeightRow {
  unsigned char count;
  BC_RigInfluence infl[BC_RIG_MAX_WEIGHTS];
} BC_PlayWeightRow;
static BC_PlayWeightRow bc_auto_weights[BC_PLAY_MAX_VERTS];
static char bc_obj_line[BC_PLAY_OBJ_LINE];
static unsigned int bc_auto_vertex_total=0U;

static int bc_play_weight_read(FILE *source,unsigned int bones)
{
  unsigned int count=0U,mesh=0U,verts=0U,base=0U,total=0U;
  unsigned int lastmesh=0U,mi,vi,grp,idx,i,k,seen=0U;
  int w,first=1;
  char *col[5],*p,*e;
  memset(bc_auto_weights,0,sizeof(bc_auto_weights));
  for(;;){
    if(!fgets(bc_obj_line,BC_PLAY_OBJ_LINE,source)){
      if(ferror(source))return -1;
      break;
    }
    if(bc_obj_line[0]=='#'){
      if(sscanf(bc_obj_line,"# mesh %u vertices=%u",&mi,&verts)==2){
        if((first && mi!=0U)||(!first && mi!=lastmesh+1U))return -1;
        if(!first){
          if(total>BC_PLAY_MAX_VERTS-count)return -1;
          total+=count;
        }
        base=total;
        count=verts;
        if(count>BC_PLAY_MAX_VERTS-base)return -1;
        lastmesh=mi; first=0;
      }
      continue;
    }
    if(first)return -1;
    p=bc_obj_line;
    for(i=0U;i<5U;i++){
      col[i]=p;
      if(i!=4U){
        e=strchr(p,'\t');if(!e)return -1;
        *e=0;p=e+1;
      }else{
        e=strpbrk(p,"\r\n");if(e)*e=0;
      }
    }
    if(bc_play_uint(col[0],&mesh)!=0 ||mesh!=lastmesh ||
       bc_play_uint(col[1],&vi)!=0 ||vi>=count ||
       bc_play_uint(col[2],&grp)!=0 ||
       bc_parse_q16(col[4],&w)!=BC_OK ||w<0 ||w>BC_RIG_ONE)return -1;
    idx=BC_PLAY_BONES;
    for(k=0U;k<bones;k++)if(strcmp(col[3],bc_play_bones[k].name)==0){idx=k;break;}
    if(idx==BC_PLAY_BONES)continue; /* non-bone vertex group */
    if(base+vi>=BC_PLAY_MAX_VERTS)return -1;
    if(bc_auto_weights[base+vi].count>=BC_RIG_MAX_WEIGHTS)return -1;
    i=(unsigned int)bc_auto_weights[base+vi].count;
    bc_auto_weights[base+vi].infl[i].bone=idx;
    bc_auto_weights[base+vi].infl[i].weight=w;
    bc_auto_weights[base+vi].count=(unsigned char)(i+1U);
    seen++;
  }
  if(first ||total>BC_PLAY_MAX_VERTS-count)return -1;
  bc_auto_vertex_total=total+count;
  return (seen && bc_auto_vertex_total)?0:-1;
}
static int bc_play_obj(FILE *in,FILE *out,unsigned int bones)
{
  unsigned int i=0U,k;
  char *p,*t;int pos[3],dst[3];
  while(fgets(bc_obj_line,BC_PLAY_OBJ_LINE,in)){
    if(bc_obj_line[0]=='v' &&bc_obj_line[1]==' '){
      if(i>=bc_auto_vertex_total)return -1;
      p=bc_obj_line+2;
      for(k=0U;k<3U;k++){
        while(*p==' '||*p=='\t')p++;
        t=p;
        while(*p && *p!=' ' && *p!='\t' && *p!='\r' && *p!='\n')p++;
        if(*p){*p=0;p++;}
        if(bc_parse_q16(t,&pos[k])!=BC_OK)return -1;
      }
      if(bc_rig_skin_point(bc_play_palette,bones,pos,
                           bc_auto_weights[i].infl,
                           (unsigned int)bc_auto_weights[i].count,dst)!=BC_RIG_OK)return -1;
      if(fputs("v ",out)==EOF)return -1;
      for(k=0U;k<3U;k++){
        if(k &&fputc(' ',out)==EOF)return -1;
        bc_print_q16(out,dst[k]);
      }
      if(fputc('\n',out)==EOF)return -1;
      i++;
    }else{
      if(fputs(bc_obj_line,out)==EOF)return -1;
    }
  }
  return (ferror(in)||ferror(out)||i!=bc_auto_vertex_total)?-1:0;
}
/* Blender Armature modifier coordinates: mesh-local -> armature-local ->
   deform -> mesh-local. Only for eligible unparented, unconstrained rigs. */
static int bc_play_transform_palette(BC_File *blend,unsigned int bones)
{
  BC_RigMat mesh,arm,im,ia,to_arm,to_mesh,tmp,result;
  unsigned int i;
  int status=bc_object_playback_spaces(blend,&mesh,&arm);
  if(status==BC_UNSUPPORTED)return 0; /* legacy synthetic .blend has no Objects */
  if(status!=BC_OK ||bc_rig_inverse_affine(&mesh,&im)!=BC_RIG_OK ||
     bc_rig_inverse_affine(&arm,&ia)!=BC_RIG_OK ||
     bc_rig_mat_mul(&ia,&mesh,&to_arm)!=BC_RIG_OK ||
     bc_rig_mat_mul(&im,&arm,&to_mesh)!=BC_RIG_OK)return -1;
  for(i=0U;i<bones;i++){
    if(bc_rig_mat_mul(&bc_play_palette[i],&to_arm,&tmp)!=BC_RIG_OK ||
       bc_rig_mat_mul(&to_mesh,&tmp,&result)!=BC_RIG_OK)return -1;
    bc_play_palette[i]=result;
  }
  return 0;
}
int bc_play_obj_frame_bound(BC_File *blend,int frame,const char *output_path,
                            unsigned int action,unsigned int slot,int filter_slot)
{
  FILE *rest=NULL,*samples=NULL,*weights=NULL,*obj=NULL,*out=NULL;
  unsigned int count=0U;
  int result=BC_FAILURE,created=0;
  rest=tmpfile();if(!rest)goto done;
  samples=tmpfile();if(!samples)goto done;
  weights=tmpfile();if(!weights)goto done;
  obj=tmpfile();if(!obj)goto done;
  if(bc_export_rest_matrices(blend,rest)!=BC_OK ||
     bc_export_sample(blend,samples,frame)!=BC_OK ||
     bc_export_weights(blend,weights)!=BC_OK ||
     bc_extract_obj(blend,obj)!=BC_OK ||
     fflush(rest)!=0 ||fflush(samples)!=0 ||
     fflush(weights)!=0 ||fflush(obj)!=0 ||
     fseek(rest,0,SEEK_SET)!=0 ||fseek(samples,0,SEEK_SET)!=0 ||
     fseek(weights,0,SEEK_SET)!=0 ||fseek(obj,0,SEEK_SET)!=0 ||
     bc_constraints_guard_global(blend)!=BC_OK ||
     bc_play_rest(rest,&count)!=0 ||bc_play_pose_metadata(blend,count)!=0 ||
     bc_play_samples(samples,count,action,slot,filter_slot)!=0 ||
     bc_play_pose(count)!=0 ||bc_play_transform_palette(blend,count)!=0 ||
     bc_play_weight_read(weights,count)!=0)goto done;
  out=fopen(output_path,"wb");if(!out)goto done;
  created=1;
  if(bc_play_obj(obj,out,count)!=0)goto done;
  result=BC_OK;
done:
  if(out && fclose(out)!=0)result=BC_FAILURE;
  if(obj)fclose(obj);
  if(weights)fclose(weights);
  if(samples)fclose(samples);
  if(rest)fclose(rest);
  if(result!=BC_OK && created)remove(output_path);
  return result;
}

int bc_play_obj_frame(BC_File *blend,int frame,const char *output_path,unsigned int action)
{
  return bc_play_obj_frame_bound(blend,frame,output_path,action,0U,0);
}

/* Per-mesh weight extraction. The global OBJ and skin tables are enumerated
   in the same Mesh-ID order; isolate one section, index vertices from zero. */
static int bc_play_weight_selected(FILE *source,unsigned int bones,unsigned int target)
{
  unsigned int count=0U,mi,verts,mesh,vi,grp,idx,k,i,seen=0U;
  int w,active=0,found=0;
  char *col[5],*p,*e;
  memset(bc_auto_weights,0,sizeof(bc_auto_weights));
  for(;;){
    if(!fgets(bc_obj_line,BC_PLAY_OBJ_LINE,source)){
      if(ferror(source))return -1;
      break;
    }
    if(bc_obj_line[0]=='#'){
      if(sscanf(bc_obj_line,"# mesh %u vertices=%u",&mi,&verts)==2){
        active=(mi==target);
        if(active){
          if(found ||verts>BC_PLAY_MAX_VERTS)return -1;
          found=1;count=verts;
        }
      }
      continue;
    }
    if(!active)continue;
    p=bc_obj_line;
    for(i=0U;i<5U;i++){
      col[i]=p;
      if(i<4U){e=strchr(p,'\t');if(!e)return -1;*e=0;p=e+1;}
      else {e=strpbrk(p,"\r\n");if(e)*e=0;}
    }
    if(bc_play_uint(col[0],&mesh)!=0 ||mesh!=target ||
       bc_play_uint(col[1],&vi)!=0 ||vi>=count ||
       bc_play_uint(col[2],&grp)!=0 ||
       bc_parse_q16(col[4],&w)!=BC_OK ||w<0 ||w>BC_RIG_ONE)return -1;
    idx=BC_PLAY_BONES;
    for(k=0U;k<bones;k++)if(strcmp(col[3],bc_play_bones[k].name)==0){idx=k;break;}
    if(idx==BC_PLAY_BONES)continue; /* unrelated, non-bone group */
    if(bc_auto_weights[vi].count>=BC_RIG_MAX_WEIGHTS)return -1;
    i=bc_auto_weights[vi].count;
    bc_auto_weights[vi].infl[i].bone=idx;
    bc_auto_weights[vi].infl[i].weight=w;
    bc_auto_weights[vi].count=(unsigned char)(i+1U);
    seen++;
  }
  bc_auto_vertex_total=count;
  return (found &&seen && count)?0:-1;
}
static int bc_play_transform_pair(unsigned int bones,const BC_RigPair *pair)
{
  BC_RigMat im,ia,to_arm,to_mesh,tmp,res;
  unsigned int i;
  if(bc_rig_inverse_affine(&pair->mesh_local,&im)!=BC_RIG_OK ||
     bc_rig_inverse_affine(&pair->arm_local,&ia)!=BC_RIG_OK ||
     bc_rig_mat_mul(&ia,&pair->mesh_local,&to_arm)!=BC_RIG_OK ||
     bc_rig_mat_mul(&im,&pair->arm_local,&to_mesh)!=BC_RIG_OK)return -1;
  for(i=0U;i<bones;i++){
    if(bc_rig_mat_mul(&bc_play_palette[i],&to_arm,&tmp)!=BC_RIG_OK ||
       bc_rig_mat_mul(&to_mesh,&tmp,&res)!=BC_RIG_OK)return -1;
    bc_play_palette[i]=res;
  }
  return 0;
}
int bc_play_rig_pair_frame(BC_File *blend,int scene_frame,const char *output,
                           const BC_RigPair *pair)
{
  FILE *rest=NULL,*samples=NULL,*weights=NULL,*obj=NULL,*out=NULL;
  unsigned int bones=0U,aid=0U,slot=0U;
  int frame=scene_frame,filter_slot=0,created=0,result=BC_FAILURE;
  if(bc_find_armature_action(blend,pair->arm_object_at,&frame,&aid,&slot,&filter_slot)!=BC_OK)
    return BC_UNSUPPORTED;
  rest=tmpfile();samples=tmpfile();weights=tmpfile();obj=tmpfile();
  if(!rest ||!samples ||!weights ||!obj)goto done;
  if(bc_export_rest_matrices(blend,rest)!=BC_OK ||
     bc_export_sample(blend,samples,frame)!=BC_OK ||
     bc_export_weights(blend,weights)!=BC_OK ||
     bc_extract_obj_mesh(blend,obj,pair->mesh_index)!=BC_OK ||
     fflush(rest)!=0 ||fflush(samples)!=0 ||fflush(weights)!=0 ||fflush(obj)!=0 ||
     fseek(rest,0,SEEK_SET)!=0 ||fseek(samples,0,SEEK_SET)!=0 ||
     fseek(weights,0,SEEK_SET)!=0 ||fseek(obj,0,SEEK_SET)!=0 ||
     bc_play_rest_select(rest,&bones,pair->arm_index)!=0 ||
     bc_play_pose_metadata_select(blend,bones,(int)pair->arm_object_index)!=0 ||
     bc_play_constraints_setup(blend,pair->arm_object_at,bones,&pair->arm_local)!=BC_OK ||
     (bc_constraints_has_ik() &&bc_play_ik_lengths(blend,pair->arm_index,bones)!=0) ||
     bc_play_samples(samples,bones,aid,slot,filter_slot)!=0 ||
     bc_play_pose(bones)!=0 ||bc_play_transform_pair(bones,pair)!=0 ||
     bc_play_weight_selected(weights,bones,pair->mesh_index)!=0)goto done;
  out=fopen(output,"wb");if(!out)goto done;created=1;
  if(bc_play_obj(obj,out,bones)!=0)goto done;
  result=BC_OK;
done:
  if(out &&fclose(out)!=0)result=BC_FAILURE;
  if(obj)fclose(obj);
  if(weights)fclose(weights);
  if(samples)fclose(samples);
  if(rest)fclose(rest);
  if(result!=BC_OK &&created)remove(output);
  return result;
}
