/* Independent multi-link FABRIK-style positional IK for a C89 32-bit Q16.16
   runtime. Fixed storage only. Rigid rest frames, no stretch. This is NOT
   Blender's IK solver, and does NOT implement Blender's per-axis IK limits. */
#include "bc_ik_chain.h"
#include "bc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int ck_add(int a,int b,int *out){return bc_qadd_checked(a,b,out);}
static int ck_sub(int a,int b,int *out){
  if(b==INT_MIN)return BC_RIG_BAD;
  return ck_add(a,-b,out);
}
static int ck_mul(int a,int b,int *out){return bc_qmul_checked(a,b,out);}
static int ck_div(int a,int b,int *out){return bc_qdiv_checked(a,b,out);}
static int vec_sub(const int a[3],const int b[3],int out[3]){
  int v[3];unsigned int k;
  for(k=0U;k<3U;k++)if(ck_sub(a[k],b[k],&v[k]))return BC_RIG_BAD;
  memcpy(out,v,sizeof(v));return BC_RIG_OK;
}
static int vec_dot(const int a[3],const int b[3],int *out){
  int s=0,v;unsigned int k;
  for(k=0U;k<3U;k++)if(ck_mul(a[k],b[k],&v)||ck_add(s,v,&s))return BC_RIG_BAD;
  *out=s;return BC_RIG_OK;
}
static int vec_cross(const int a[3],const int b[3],int out[3]){
  unsigned int i;int p,q,t[3];
  for(i=0U;i<3U;i++){
    unsigned int j=(i+1U)%3U,k=(i+2U)%3U;
    if(ck_mul(a[j],b[k],&p)||ck_mul(a[k],b[j],&q)||ck_sub(p,q,&t[i]))return BC_RIG_BAD;
  }
  memcpy(out,t,sizeof(t));return BC_RIG_OK;
}
/* sqrt(value Q16) with 32-bit-only Newton steps, checked at each step. */
static int qsqrt(int value,int *out){
  int x,y,z;unsigned int i;
  if(value<0)return BC_RIG_BAD;
  if(value==0){*out=0;return BC_RIG_OK;}
  x=value>BC_RIG_ONE?value:BC_RIG_ONE;
  for(i=0U;i<40U;i++){
    if(ck_div(value,x,&y)||ck_add(x,y,&z))return BC_RIG_BAD;
    x=z/2;
    if(x<=0)return BC_RIG_BAD;
  }
  *out=x;return BC_RIG_OK;
}
static int vec_norm(const int a[3],int *out){
  int s,v;
  if(vec_dot(a,a,&s)||s<=0||qsqrt(s,&v))return BC_RIG_UNSUPPORTED;
  *out=v;return BC_RIG_OK;
}
static int vec_unit(const int a[3],int out[3],int *length){
  int n,v[3];unsigned int i;
  if(vec_norm(a,&n)||n<64)return BC_RIG_UNSUPPORTED;
  for(i=0U;i<3U;i++)if(ck_div(a[i],n,&v[i]))return BC_RIG_BAD;
  memcpy(out,v,sizeof(v));
  if(length)*length=n;
  return BC_RIG_OK;
}
static int vec_length(const int a[3],const int b[3],int *out){
  int d[3],s;
  if(vec_sub(a,b,d)||vec_dot(d,d,&s)||s<0)return BC_RIG_BAD;
  if(s==0){*out=0;return BC_RIG_OK;}
  return qsqrt(s,out);
}
static int vec_place(const int p[3],const int u[3],int length,int out[3]){
  int r[3],d;unsigned int i;
  for(i=0U;i<3U;i++)if(ck_mul(u[i],length,&d)||ck_add(p[i],d,&r[i]))return BC_RIG_BAD;
  memcpy(out,r,sizeof(r));return BC_RIG_OK;
}
static int vec_cone(const int previous[3],const int candidate[3],int threshold,int out[3]){
  int pv[3],cd[3],perp[3],bend[3],unit[3];
  int dot,norm,si,csq,ssq,v,sum;unsigned int k;
  if(threshold<=-BC_RIG_ONE){memcpy(out,candidate,sizeof(int)*3U);return BC_RIG_OK;}
  if(threshold>BC_RIG_ONE)return BC_RIG_BAD;
  if(vec_unit(previous,pv,NULL)||vec_unit(candidate,cd,NULL)||
     vec_dot(pv,cd,&dot))return BC_RIG_UNSUPPORTED;
  if(dot>=threshold-64){memcpy(out,cd,sizeof(cd));return BC_RIG_OK;}
  for(k=0U;k<3U;k++){
    if(ck_mul(pv[k],dot,&v)||ck_sub(cd[k],v,&perp[k]))return BC_RIG_BAD;
  }
  if(vec_unit(perp,unit,&norm))return BC_RIG_UNSUPPORTED;
  if(ck_mul(threshold,threshold,&csq)||ck_sub(BC_RIG_ONE,csq,&ssq))return BC_RIG_BAD;
  if(ssq<0 && ssq>=-8)ssq=0;
  if(ssq<0 ||qsqrt(ssq,&si))return BC_RIG_BAD;
  for(k=0U;k<3U;k++){
    if(ck_mul(threshold,pv[k],&v)||ck_mul(si,unit[k],&sum)||
       ck_add(v,sum,&bend[k]))return BC_RIG_BAD;
  }
  if(vec_unit(bend,out,NULL))return BC_RIG_UNSUPPORTED;
  return BC_RIG_OK;
}
static int near(int a,int b,int tol){
  int d;
  return ck_sub(a,b,&d)==BC_RIG_OK &&d>=-tol&&d<=tol;
}
/* Return a normalized shortest-arc quaternion, or reject ambiguous 180deg. */
static int swing_rotation(const int restdir[3],const int posedirection[3],
                          BC_RigMat *rot){
  int a[3],b[3],v[3],q[4],dot,sum,n2,n,t;unsigned int k;
  const int zero[3]={0,0,0};int one[3]={BC_RIG_ONE,BC_RIG_ONE,BC_RIG_ONE};
  if(vec_unit(restdir,a,NULL)||vec_unit(posedirection,b,NULL)||
     vec_dot(a,b,&dot))return BC_RIG_UNSUPPORTED;
  if(dot>BC_RIG_ONE)dot=BC_RIG_ONE;
  if(dot< -BC_RIG_ONE+256)return BC_RIG_UNSUPPORTED;
  if(vec_cross(a,b,v)||ck_add(BC_RIG_ONE,dot,&q[0]))return BC_RIG_BAD;
  for(k=0U;k<3U;k++)q[k+1U]=v[k];
  n2=0;
  for(k=0U;k<4U;k++)if(ck_mul(q[k],q[k],&t)||ck_add(n2,t,&n2))return BC_RIG_BAD;
  if(n2<=0||qsqrt(n2,&n)||n<64)return BC_RIG_UNSUPPORTED;
  for(k=0U;k<4U;k++){
    if(ck_div(q[k],n,&sum))return BC_RIG_BAD;
    q[k]=sum;
  }
  return bc_rig_mat_from_trs(zero,q,one,rot);
}
static int check_rest(const BC_IKChainGoal *g,int length[BC_CHAIN_MAX_LINKS]){
  unsigned int i,j;int d[3],len,axis,m;
  BC_RigMat inv;
  if(g->count<3U||g->count>BC_CHAIN_MAX_LINKS||
     g->iterations==0U||g->iterations>BC_CHAIN_MAX_ITERATIONS)return BC_RIG_UNSUPPORTED;
  for(i=0U;i<=g->count;i++)for(j=0U;j<3U;j++){
    if(g->rest_joint[i][j]>32*BC_RIG_ONE||g->rest_joint[i][j]<-32*BC_RIG_ONE)
      return BC_RIG_UNSUPPORTED;
  }
  for(j=0U;j<3U;j++)if(g->target[j]>32*BC_RIG_ONE||g->target[j]<-32*BC_RIG_ONE)
    return BC_RIG_UNSUPPORTED;
  for(i=0U;i<g->count;i++){
    if(bc_rig_inverse_rigid(&g->rest[i],&inv)!=BC_RIG_OK||
       vec_sub(g->rest_joint[i+1U],g->rest_joint[i],d)||
       vec_norm(d,&len)!=BC_RIG_OK||len<BC_RIG_ONE/128||len>16*BC_RIG_ONE)
      return BC_RIG_UNSUPPORTED;
    length[i]=len;
    for(j=0U;j<3U;j++){
      if(!near(g->rest[i].m[4U*j+3U],g->rest_joint[i][j],192)||
         ck_mul(g->rest[i].m[4U*j+1U],len,&axis)!=BC_RIG_OK||
         !near(d[j],axis,256))return BC_RIG_UNSUPPORTED;
    }
    if(g->cosine_limit[i]<-BC_RIG_ONE||g->cosine_limit[i]>BC_RIG_ONE)
      return BC_RIG_UNSUPPORTED;
  }
  if(g->cosine_limit[0]!=-BC_RIG_ONE)return BC_RIG_UNSUPPORTED;
  m=0;
  for(i=0U;i<g->count;i++)if(ck_add(m,length[i],&m)||m>32*BC_RIG_ONE)
    return BC_RIG_UNSUPPORTED;
  if(vec_length(g->rest_joint[0],g->target,&len)||len>m+BC_CHAIN_TOLERANCE)
    return BC_RIG_UNSUPPORTED;
  return BC_RIG_OK;
}
int bc_ik_chain_solve(const BC_IKChainGoal *g,BC_IKChainResult *dst){
  BC_IKChainResult tmp;BC_RigMat rot,posed,inverse;
  int len[BC_CHAIN_MAX_LINKS],u[3],p[3],prev[3],d[3],distance=0,angle;
  unsigned int i,iter,k;int good=0;
  if(!g||!dst||check_rest(g,len)!=BC_RIG_OK){
    return BC_RIG_UNSUPPORTED;
  }
  memset(&tmp,0,sizeof(tmp));tmp.count=g->count;
  for(i=0U;i<=g->count;i++)memcpy(tmp.joint[i],g->rest_joint[i],sizeof(int)*3U);
  for(iter=0U;iter<g->iterations;iter++){
    /* Forward: place the tip on target and enforce segment lengths backward. */
    memcpy(tmp.joint[g->count],g->target,sizeof(int)*3U);
    for(i=g->count;i>0U;i--){
      if(vec_sub(tmp.joint[i-1U],tmp.joint[i],d)||
         vec_unit(d,u,NULL)||vec_place(tmp.joint[i],u,len[i-1U],p)){
        return BC_RIG_UNSUPPORTED;
      }
      memcpy(tmp.joint[i-1U],p,sizeof(p));
    }
    /* Backward: lock root, and clamp each outgoing joint to a bend cone. */
    memcpy(tmp.joint[0],g->rest_joint[0],sizeof(int)*3U);
    for(i=0U;i<g->count;i++){
      if(vec_sub(tmp.joint[i+1U],tmp.joint[i],d)||vec_unit(d,u,NULL)){
        return BC_RIG_UNSUPPORTED;
      }
      if(i>0U &&g->cosine_limit[i]>-BC_RIG_ONE){
        if(vec_sub(tmp.joint[i],tmp.joint[i-1U],prev)||
           vec_cone(prev,u,g->cosine_limit[i],u))return BC_RIG_UNSUPPORTED;
      }
      if(vec_place(tmp.joint[i],u,len[i],p))return BC_RIG_BAD;
      memcpy(tmp.joint[i+1U],p,sizeof(p));
    }
    if(vec_length(tmp.joint[g->count],g->target,&distance))return BC_RIG_BAD;
    if(distance<=BC_CHAIN_TOLERANCE){
      /* Verify constraints after *all* relaxation, not just the final step. */
      for(i=1U;i<g->count;i++)if(g->cosine_limit[i]>-BC_RIG_ONE){
        int a[3],b[3],ua[3],ub[3];
        if(vec_sub(tmp.joint[i],tmp.joint[i-1U],a)||
           vec_sub(tmp.joint[i+1U],tmp.joint[i],b)||
           vec_unit(a,ua,NULL)||vec_unit(b,ub,NULL)||vec_dot(ua,ub,&angle))
          return BC_RIG_UNSUPPORTED;
        if(angle<g->cosine_limit[i]-512)return BC_RIG_UNSUPPORTED;
      }
      good=1;tmp.iterations_used=iter+1U;tmp.residual=distance;break;
    }
  }
  if(!good)return BC_RIG_UNSUPPORTED;
  /* Build per-bone armature-space pose and rest->pose deform matrices. */
  for(i=0U;i<g->count;i++){
    int rest_dir[3],pose_dir[3];
    if(vec_sub(g->rest_joint[i+1U],g->rest_joint[i],rest_dir)||
       vec_sub(tmp.joint[i+1U],tmp.joint[i],pose_dir)||
       swing_rotation(rest_dir,pose_dir,&rot)!=BC_RIG_OK||
       bc_rig_mat_mul(&rot,&g->rest[i],&posed)!=BC_RIG_OK||
       bc_rig_inverse_rigid(&g->rest[i],&inverse)!=BC_RIG_OK)
      {
        return BC_RIG_UNSUPPORTED;
      }
    for(k=0U;k<3U;k++)posed.m[4U*k+3U]=tmp.joint[i][k];
    tmp.pose[i]=posed;
    if(bc_rig_mat_mul(&posed,&inverse,&tmp.deform[i])!=BC_RIG_OK){
      return BC_RIG_BAD;
    }
  }
  *dst=tmp;return BC_RIG_OK;
}

/* Strict fixed-count TSV grammar with serialised rigid rest matrices.
   Input records (in order):
     chain N
     target X Y Z
     iterations N
     node INDEX X Y Z          (N+1 lines)
     limit INDEX MIN_COSINE     (N-1 lines, indices 1..N-1)
     matrix INDEX m00 ... m23  (N lines, 12 entries, row-major)
   Comments start with #. No automatic default rest basis; orientation is explicit.
*/
static int tokens(FILE *f,char t[14][64],unsigned int want){
  char line[1024],*p;unsigned int n;
  while(fgets(line,sizeof(line),f)){
    if(!strchr(line,'\n')&&!feof(f))return BC_RIG_BAD;
    n=0U;p=strtok(line," \t\r\n");
    if(!p||p[0]=='#')continue;
    while(p){
      if(n>=want||strlen(p)>=64U)return BC_RIG_BAD;
      strcpy(t[n],p);n++;p=strtok(NULL," \t\r\n");
    }
    return n==want?BC_RIG_OK:BC_RIG_BAD;
  }
  return BC_RIG_BAD;
}
static int parse_uint(const char *s,unsigned int *out){
  unsigned int n=0U, digit;
  if(!*s)return BC_RIG_BAD;
  while(*s){
    if(*s<'0'||*s>'9')return BC_RIG_BAD;
    digit=(unsigned int)(*s-'0');
    if(n>10000U)return BC_RIG_BAD;
    n=n*10U+digit;s++;
  }
  *out=n;return BC_RIG_OK;
}
static int goal_file(FILE *f,BC_IKChainGoal *g){
  char t[14][64];int ch;unsigned int i,k,n;
  memset(g,0,sizeof(*g));
  if(tokens(f,t,2U)||strcmp(t[0],"chain")||parse_uint(t[1],&n)||
     n<3U||n>BC_CHAIN_MAX_LINKS)return BC_RIG_BAD;
  g->count=n;
  if(tokens(f,t,4U)||strcmp(t[0],"target"))return BC_RIG_BAD;
  for(k=0U;k<3U;k++)if(bc_parse_q16(t[k+1U],&g->target[k])!=BC_OK)return BC_RIG_BAD;
  if(tokens(f,t,2U)||strcmp(t[0],"iterations")||
     parse_uint(t[1],&g->iterations)||g->iterations==0U||
     g->iterations>BC_CHAIN_MAX_ITERATIONS)return BC_RIG_BAD;
  for(i=0U;i<=n;i++){
    unsigned int index;
    if(tokens(f,t,5U)||strcmp(t[0],"node")||
       parse_uint(t[1],&index)||index!=i)return BC_RIG_BAD;
    for(k=0U;k<3U;k++)if(bc_parse_q16(t[k+2U],&g->rest_joint[i][k])!=BC_OK)
      return BC_RIG_BAD;
  }
  for(i=0U;i<n;i++)g->cosine_limit[i]=-BC_RIG_ONE;
  for(i=1U;i<n;i++){
    unsigned int index;
    if(tokens(f,t,3U)||strcmp(t[0],"limit")||
       parse_uint(t[1],&index)||index!=i||
       bc_parse_q16(t[2],&g->cosine_limit[i])!=BC_OK)
      return BC_RIG_BAD;
  }
  for(i=0U;i<n;i++){
    unsigned int index;
    if(tokens(f,t,14U)||strcmp(t[0],"matrix")||
       parse_uint(t[1],&index)||index!=i)return BC_RIG_BAD;
    bc_rig_mat_identity(&g->rest[i]);
    for(k=0U;k<12U;k++){
      if(bc_parse_q16(t[k+2U],&g->rest[i].m[4U*(k/4U)+(k%4U)])!=BC_OK)
        return BC_RIG_BAD;
    }
  }
  while((ch=fgetc(f))!=EOF){
    if(ch!=' '&&ch!='\t'&&ch!='\r'&&ch!='\n')return BC_RIG_BAD;
  }
  return ferror(f)?BC_RIG_BAD:BC_RIG_OK;
}
static int write_pose(FILE *f,unsigned int i,const BC_RigMat *m){
  unsigned int k;
  if(fprintf(f,"%u",i)<0)return BC_RIG_BAD;
  for(k=0U;k<12U;k++){
    if(fputc('\t',f)==EOF)return BC_RIG_BAD;
    bc_print_q16(f,m->m[k]);
  }
  return fputc('\n',f)==EOF?BC_RIG_BAD:BC_RIG_OK;
}
static int write_joint(FILE *f,unsigned int i,const int p[3]){
  unsigned int k;
  if(fprintf(f,"%u",i)<0)return BC_RIG_BAD;
  for(k=0U;k<3U;k++){
    if(fputc('\t',f)==EOF)return BC_RIG_BAD;
    bc_print_q16(f,p[k]);
  }
  return fputc('\n',f)==EOF?BC_RIG_BAD:BC_RIG_OK;
}
int bc_ik_chain_cli(const char *input,const char *palette,const char *joints){
  static BC_IKChainGoal g;
  static BC_IKChainResult result;
  FILE *in=NULL,*a=NULL,*b=NULL;unsigned int i;int rc=BC_RIG_BAD;
  int a_created=0,b_created=0;
  if(!input||!palette||!joints||!strcmp(input,palette)||
     !strcmp(input,joints)||!strcmp(palette,joints))return BC_RIG_BAD;
  in=fopen(input,"rb");if(!in)goto done;
  if(goal_file(in,&g)!=BC_RIG_OK||bc_ik_chain_solve(&g,&result)!=BC_RIG_OK)goto done;
  a=fopen(palette,"wb");if(!a)goto done;a_created=1;
  b=fopen(joints,"wb");if(!b)goto done;b_created=1;
  if(fputs("# bone id, row-major 3x4 Q16.16 deformation matrix\n",a)==EOF||
     fprintf(b,"# IK chain: iterations=%u residual=",result.iterations_used)<0)goto done;
  bc_print_q16(b,result.residual);
  if(fputc('\n',b)==EOF)goto done;
  for(i=0U;i<g.count;i++)if(write_pose(a,i,&result.deform[i]))goto done;
  for(i=0U;i<=g.count;i++)if(write_joint(b,i,result.joint[i]))goto done;
  if(ferror(a)||ferror(b))goto done;
  rc=BC_RIG_OK;
done:
  if(in)if(fclose(in)!=0)rc=BC_RIG_BAD;
  if(a)if(fclose(a)!=0)rc=BC_RIG_BAD;
  if(b)if(fclose(b)!=0)rc=BC_RIG_BAD;
  if(rc!=BC_RIG_OK){
    if(a_created)remove(palette);
    if(b_created)remove(joints);
  }
  return rc;
}
