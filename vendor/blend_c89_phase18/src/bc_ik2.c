/* Phase 15: original, bounded, 3D analytical two-segment IK.
 * GPL Blender source was studied to identify the separate bKinematicConstraint
 * format, but this solver is not a reimplementation of Blender's iterative IK.
 * No heap, no floating point, no 64-bit integers, no recursion.
 */
#include "bc_ik2.h"
#include "bc.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int iadd(int a,int b,int *out){return bc_qadd_checked(a,b,out);}
static int isub(int a,int b,int *out){
  if(b==INT_MIN)return BC_RIG_BAD;
  return iadd(a,-b,out);
}
static int imul(int a,int b,int *out){return bc_qmul_checked(a,b,out);}
static int idiv(int a,int b,int *out){return bc_qdiv_checked(a,b,out);}
static int iabs(int a,int *out){if(a==INT_MIN)return BC_RIG_BAD;*out=a<0?-a:a;return 0;}
static int vsub(const int a[3],const int b[3],int out[3]){
  int tmp[3];unsigned int i;
  for(i=0U;i<3U;i++)if(isub(a[i],b[i],&tmp[i]))return BC_RIG_BAD;
  memcpy(out,tmp,sizeof(tmp));return 0;
}
static int vdot(const int a[3],const int b[3],int *out){
  int sum=0,part;unsigned int i;
  for(i=0U;i<3U;i++)if(imul(a[i],b[i],&part)||iadd(sum,part,&sum))return BC_RIG_BAD;
  *out=sum;return 0;
}
static int vcross(const int a[3],const int b[3],int out[3]){
  unsigned int i;int x,y,temp[3];
  for(i=0U;i<3U;i++){
    unsigned int p=(i+1U)%3U,q=(i+2U)%3U;
    if(imul(a[p],b[q],&x)||imul(a[q],b[p],&y)||isub(x,y,&temp[i]))return BC_RIG_BAD;
  }
  memcpy(out,temp,sizeof(temp));return 0;
}
/* Newton sqrt in Q16; bounds arithmetic instead of assuming int64 intermediates. */
static int isqrt(int a,int *out){
  int x,y,z;unsigned int i;
  if(a<0)return BC_RIG_BAD;
  if(a==0){*out=0;return 0;}
  x=a>BC_RIG_ONE?a:BC_RIG_ONE;
  for(i=0U;i<32U;i++){
    if(idiv(a,x,&y)||iadd(x,y,&z))return BC_RIG_BAD;
    x=z/2;
    if(!x)return BC_RIG_BAD;
  }
  *out=x;return 0;
}
static int vunit(const int a[3],int out[3],int *len){
  int len2,l,temp[3];unsigned int i;
  if(vdot(a,a,&len2)||len2<=0||isqrt(len2,&l)||l<32)return BC_RIG_UNSUPPORTED;
  for(i=0U;i<3U;i++)if(idiv(a[i],l,&temp[i]))return BC_RIG_BAD;
  memcpy(out,temp,sizeof(temp));*len=l;return BC_RIG_OK;
}
static int make_pose(const int pos[3],const int direction[3],
                     const int normal[3],BC_RigMat *dst){
  int x[3],y[3],z[3],l;unsigned int i;
  if(vunit(direction,y,&l)||vunit(normal,z,&l)||vcross(y,z,x))return BC_RIG_BAD;
  /* Re-orthogonalize once so numerical noise does not distort bone lengths. */
  if(vunit(x,x,&l)||vcross(x,y,z))return BC_RIG_BAD;
  bc_rig_mat_identity(dst);
  for(i=0U;i<3U;i++){
    dst->m[i*4U+0U]=x[i];dst->m[i*4U+1U]=y[i];
    dst->m[i*4U+2U]=z[i];dst->m[i*4U+3U]=pos[i];
  }
  return 0;
}
int bc_ik2_solve(const BC_IK2Goal *g,BC_IK2Result *dst){
  BC_IK2Result temp;
  int delta[3],u[3],v[3],normal[3],polevec[3];
  int upper[3],lower[3],proj[3];
  int d,pl,dot,t,a,h,h2,l1sq,l2sq,dsq,num,den,lo,hi,z;
  unsigned int i;
  if(!g||!dst||g->length[0]<256||g->length[1]<256)return BC_RIG_UNSUPPORTED;
  if(vsub(g->target,g->root,delta)||vunit(delta,u,&d)||
     iadd(g->length[0],g->length[1],&hi)||
     isub(g->length[0],g->length[1],&z)||iabs(z,&lo))return BC_RIG_UNSUPPORTED;
  /* Do not stretch/clamp silently: goals out of reach require explicit caller policy. */
  if(d>hi ||d<lo)return BC_RIG_UNSUPPORTED;
  if(vsub(g->pole,g->root,polevec)||vdot(polevec,u,&dot))return BC_RIG_BAD;
  for(i=0U;i<3U;i++)
    if(imul(u[i],dot,&t)||isub(polevec[i],t,&proj[i]))return BC_RIG_BAD;
  if(vunit(proj,v,&pl)!=0||vcross(u,v,normal)!=0)return BC_RIG_UNSUPPORTED;
  if(imul(g->length[0],g->length[0],&l1sq)||
     imul(g->length[1],g->length[1],&l2sq)||imul(d,d,&dsq)||
     isub(l1sq,l2sq,&num)||iadd(num,dsq,&num)||
     iadd(d,d,&den)||idiv(num,den,&a)||imul(a,a,&z)||
     isub(l1sq,z,&h2))return BC_RIG_BAD;
  if(h2< -64)return BC_RIG_UNSUPPORTED;
  if(h2<0)h2=0;
  if(isqrt(h2,&h))return BC_RIG_BAD;
  /* Solve elbow on chosen pole side; tip remains exact target, not approximated. */
  for(i=0U;i<3U;i++){
    if(imul(u[i],a,&t)||iadd(g->root[i],t,&z)||
       imul(v[i],h,&t)||iadd(z,t,&temp.elbow[i]))return BC_RIG_BAD;
    temp.root[i]=g->root[i];temp.tip[i]=g->target[i];
  }
  if(vsub(temp.elbow,temp.root,upper)||
     vsub(temp.tip,temp.elbow,lower))return BC_RIG_BAD;
  if(make_pose(temp.root,upper,normal,&temp.pose[0])||
     make_pose(temp.elbow,lower,normal,&temp.pose[1]))return BC_RIG_BAD;
  bc_rig_mat_identity(&temp.rest[0]);
  bc_rig_mat_identity(&temp.rest[1]);
  for(i=0U;i<3U;i++)temp.rest[0].m[i*4U+3U]=temp.root[i];
  /* Canonical straight rest: the second bone is located +Y by upper length. */
  temp.rest[1].m[3]=temp.root[0];
  if(iadd(temp.root[1],g->length[0],&temp.rest[1].m[7]))return BC_RIG_BAD;
  temp.rest[1].m[11]=temp.root[2];
  if(bc_rig_inverse_affine(&temp.rest[0],&temp.deform[0])||
     bc_rig_mat_mul(&temp.pose[0],&temp.deform[0],&temp.deform[0])||
     bc_rig_inverse_affine(&temp.rest[1],&temp.deform[1])||
     bc_rig_mat_mul(&temp.pose[1],&temp.deform[1],&temp.deform[1]))return BC_RIG_BAD;
  *dst=temp;return BC_RIG_OK;
}

/* Phase 17: rest-space orientation-aware IK. All vectors are Q16.16.
 * Blender Bone.arm_mat is a rigid armature-space rest basis; its local Y axis
 * follows the serialized head/tail segment. We calculate a minimal swing from
 * the rest segment to the IK segment and apply it to the original rest basis.
 * This preserves bone roll (unlike regenerating a world-up look-at matrix).
 * Twist solving, pole-angle semantics and Blender iterative IK are NOT claimed.
 */
int bc_ik2_segment_length(const int head[3],const int tail[3],int *len)
{
  int d[3],u[3],l;
  if(!head||!tail||!len ||vsub(tail,head,d)!=BC_RIG_OK ||
     vunit(d,u,&l)!=BC_RIG_OK)return BC_RIG_UNSUPPORTED;
  *len=l;return BC_RIG_OK;
}
static int ik17_near(int a,int b,int tolerance)
{
  int delta;
  if(isub(a,b,&delta)!=BC_RIG_OK)return 0;
  return delta>=-tolerance &&delta<=tolerance;
}
/* Small rigid swing of source direction into target direction.
   Quaternion [1+dot, cross] avoids acos/sin and 64-bit math.
   A deterministic 180deg rotation about the rest local X axis is used only
   for a proven antiparallel direction (not a merely nearly opposite vector). */
static int ik17_swing(const int source[3],const int dest[3],
                      const BC_RigMat *rest,BC_RigMat *rotation)
{
  int src[3],dst[3],v[3],dot,ls,ld,q[4],n2,n,term,scale[3];
  unsigned int k;
  const int zero[3]={0,0,0};
  if(vunit(source,src,&ls)!=BC_RIG_OK ||
     vunit(dest,dst,&ld)!=BC_RIG_OK ||
     vdot(src,dst,&dot)!=BC_RIG_OK)return BC_RIG_UNSUPPORTED;
  if(dot>BC_RIG_ONE)dot=BC_RIG_ONE;
  if(dot< -BC_RIG_ONE)dot= -BC_RIG_ONE;
  if(dot< -BC_RIG_ONE+128){
    int axis[3],sum[3],ln;
    /* No guessed flip when input is merely near 180 degrees. */
    for(k=0U;k<3U;k++){
      axis[k]=rest->m[k*4U];
      if(iadd(src[k],dst[k],&sum[k])!=BC_RIG_OK ||
         !ik17_near(sum[k],0,96))return BC_RIG_UNSUPPORTED;
    }
    if(vunit(axis,axis,&ln)!=BC_RIG_OK)return BC_RIG_UNSUPPORTED;
    q[0]=0;
    for(k=0U;k<3U;k++)q[k+1U]=axis[k];
  }else{
    if(vcross(src,dst,v)!=BC_RIG_OK ||
       iadd(BC_RIG_ONE,dot,&q[0])!=BC_RIG_OK)return BC_RIG_BAD;
    for(k=0U;k<3U;k++)q[k+1U]=v[k];
    n2=0;
    for(k=0U;k<4U;k++){
      if(imul(q[k],q[k],&term)!=BC_RIG_OK ||
         iadd(n2,term,&n2)!=BC_RIG_OK)return BC_RIG_BAD;
    }
    if(n2<=0 ||isqrt(n2,&n)!=BC_RIG_OK ||n<32)return BC_RIG_UNSUPPORTED;
    for(k=0U;k<4U;k++){
      int x;
      if(idiv(q[k],n,&x)!=BC_RIG_OK)return BC_RIG_BAD;
      q[k]=x;
    }
  }
  for(k=0U;k<3U;k++)scale[k]=BC_RIG_ONE;
  return bc_rig_mat_from_trs(zero,q,scale,rotation);
}
static int ik17_pose(const BC_RigMat *rest,const int from[3],
                     const int to[3],const int place[3],BC_RigMat *out)
{
  BC_RigMat rot,result;
  unsigned int i,j;
  if(ik17_swing(from,to,rest,&rot)!=BC_RIG_OK ||
     bc_rig_mat_mul(&rot,rest,&result)!=BC_RIG_OK)return BC_RIG_UNSUPPORTED;
  for(i=0U;i<3U;i++){
    for(j=0U;j<3U;j++)out->m[4U*i+j]=result.m[4U*i+j];
    out->m[4U*i+3U]=place[i];
  }
  for(i=0U;i<3U;i++)out->m[12U+i]=0;
  out->m[15]=BC_RIG_ONE;
  return BC_RIG_OK;
}
int bc_ik2_solve_rest(const BC_IK2Goal *g,const BC_RigMat rest[2],
                      const int *head,const int *tail,
                      BC_IK2Result *dst)
{
  BC_IK2Result result;
  BC_RigMat inverse[2];
  int dir[2][3],wanted[2][3],length;
  unsigned int k,j;
  if(!g ||!rest ||!head ||!tail ||!dst)return BC_RIG_BAD;
  for(k=0U;k<2U;k++){
    if(bc_rig_inverse_rigid(&rest[k],&inverse[k])!=BC_RIG_OK ||
       vsub(tail+3U*k,head+3U*k,dir[k])!=BC_RIG_OK ||
       bc_ik2_segment_length(head+3U*k,tail+3U*k,&length)!=BC_RIG_OK ||
       !ik17_near(length,g->length[k],160))return BC_RIG_UNSUPPORTED;
    /* Columns 1 represent Blender's local Y axis in armature space. */
    for(j=0U;j<3U;j++){
      int expected;
      if(!ik17_near(rest[k].m[4U*j+3U],head[3U*k+j],192) ||
         imul(rest[k].m[4U*j+1U],length,&expected)!=BC_RIG_OK ||
         !ik17_near(dir[k][j],expected,192))return BC_RIG_UNSUPPORTED;
    }
  }
  for(j=0U;j<3U;j++)if(!ik17_near(tail[j],head[3U+j],192))
    return BC_RIG_UNSUPPORTED;
  if(bc_ik2_solve(g,&result)!=BC_RIG_OK ||
     vsub(result.elbow,result.root,wanted[0])!=BC_RIG_OK ||
     vsub(result.tip,result.elbow,wanted[1])!=BC_RIG_OK ||
     ik17_pose(&rest[0],dir[0],wanted[0],result.root,&result.pose[0])!=BC_RIG_OK ||
     ik17_pose(&rest[1],dir[1],wanted[1],result.elbow,&result.pose[1])!=BC_RIG_OK)
    return BC_RIG_UNSUPPORTED;
  for(k=0U;k<2U;k++){
    result.rest[k]=rest[k];
    if(bc_rig_mat_mul(&result.pose[k],&inverse[k],&result.deform[k])!=BC_RIG_OK)
      return BC_RIG_BAD;
  }
  *dst=result;
  return BC_RIG_OK;
}

static int ik_next(FILE *f,char t[4][64],unsigned int want){
  char line[512];char *tok;unsigned int n;
  if(!fgets(line,sizeof(line),f))return BC_RIG_BAD;
  if(!strchr(line,'\n') && !feof(f))return BC_RIG_BAD;
  tok=strtok(line," \t\r\n");n=0U;
  while(tok){
    if(n>=want||strlen(tok)>=64U)return BC_RIG_BAD;
    strcpy(t[n++],tok);tok=strtok(NULL," \t\r\n");
  }
  return n==want?BC_RIG_OK:BC_RIG_BAD;
}
static int ik_goal(FILE *f,BC_IK2Goal *g){
  char t[4][64];unsigned int i;
  const char *names[4]={"root","target","pole","lengths"};
  int *positions[3];
  positions[0]=g->root;positions[1]=g->target;positions[2]=g->pole;
  for(i=0U;i<4U;i++){
    unsigned int j,n=i==3U?3U:4U;
    if(ik_next(f,t,n)!=BC_RIG_OK ||strcmp(t[0],names[i]))return BC_RIG_BAD;
    for(j=1U;j<n;j++)if(bc_parse_q16(t[j],
        i==3U?&g->length[j-1U]:&positions[i][j-1U])!=BC_OK)return BC_RIG_BAD;
  }
  /* Extra non-whitespace input is invalid rather than ignored. */
  {
    int ch;
    while((ch=fgetc(f))!=EOF)if(ch!=' '&&ch!='\t'&&ch!='\n'&&ch!='\r')return BC_RIG_BAD;
  }
  return ferror(f)?BC_RIG_BAD:BC_RIG_OK;
}
static int ik_write_mat(FILE *f,unsigned int idx,const BC_RigMat *m){
  unsigned int k;
  if(fprintf(f,"%u",idx)<0)return BC_RIG_BAD;
  for(k=0U;k<12U;k++){
    if(fputc('\t',f)==EOF)return BC_RIG_BAD;
    bc_print_q16(f,m->m[k]);
  }
  return fputc('\n',f)==EOF?BC_RIG_BAD:BC_RIG_OK;
}
static int ik_write_joint(FILE *f,const char *name,const int p[3]){
  unsigned int k;
  if(fputs(name,f)==EOF)return BC_RIG_BAD;
  for(k=0U;k<3U;k++){
    if(fputc('\t',f)==EOF)return BC_RIG_BAD;
    bc_print_q16(f,p[k]);
  }
  return fputc('\n',f)==EOF?BC_RIG_BAD:BC_RIG_OK;
}
int bc_ik2_cli(const char *goal_path,const char *palette_path,const char *joints_path){
  FILE *in=NULL,*out=NULL,*joints=NULL;BC_IK2Goal g;BC_IK2Result result;
  int ret=BC_RIG_BAD,write_pal=0,write_joints=0;
  if(!goal_path||!palette_path||!joints_path||
     !strcmp(goal_path,palette_path)||!strcmp(goal_path,joints_path)||
     !strcmp(palette_path,joints_path))return BC_RIG_BAD;
  in=fopen(goal_path,"rb");if(!in)goto done;
  if(ik_goal(in,&g)||bc_ik2_solve(&g,&result))goto done;
  out=fopen(palette_path,"wb");if(!out)goto done;
  write_pal=1;
  if(fputs("# deform matrices: bone id, row-major 3x4 Q16.16\n",out)==EOF ||
     ik_write_mat(out,0U,&result.deform[0])||
     ik_write_mat(out,1U,&result.deform[1])||ferror(out))goto done;
  joints=fopen(joints_path,"wb");if(!joints)goto done;
  write_joints=1;
  if(fputs("# IK two-bone analytic joint coordinates Q16.16\n",joints)==EOF ||
     ik_write_joint(joints,"root",result.root)||
     ik_write_joint(joints,"elbow",result.elbow)||
     ik_write_joint(joints,"tip",result.tip)||ferror(joints))goto done;
  ret=BC_RIG_OK;
 done:
  if(in)fclose(in);
  if(out &&fclose(out)!=0)ret=BC_RIG_BAD;
  if(joints &&fclose(joints)!=0)ret=BC_RIG_BAD;
  if(ret!=BC_RIG_OK){
    if(write_pal)remove(palette_path);
    if(write_joints)remove(joints_path);
  }
  return ret;
}
