/* Streaming offline skinning interchange format; independent of BLEND parser.
   Matrices are DEFORMATION matrices pose * inverse(rest), NOT raw rest matrices.
   The caller resolves vertex-group names to bone indices before producing TSV. */
#include "bc.h"
#include "bc_rig_q16.h"
#include <string.h>
#include <limits.h>

#define BC_TSV_LINE 2048U
#define BC_TSV_TOKENS 32U
static BC_RigMat bc_palette[BC_RIG_MAX_BONES];
static unsigned char bc_present[BC_RIG_MAX_BONES];
static unsigned char bc_pose_seen[BC_RIG_MAX_BONES];
static char bc_line[BC_TSV_LINE];
static char *bc_tokens[BC_TSV_TOKENS];

static int bc_parse_uint(const char *s,unsigned int *value)
{
  unsigned int v=0U,d;
  const unsigned char *p=(const unsigned char *)s;
  if(*p==0)return BC_RIG_BAD;
  while(*p){
    if(*p<'0'||*p>'9')return BC_RIG_BAD;
    d=(unsigned int)(*p-'0');
    if(v>(UINT_MAX-d)/10U)return BC_RIG_BAD;
    v=v*10U+d;p++;
  }
  *value=v;return BC_RIG_OK;
}
static int bc_split(char *s,char **tokens,unsigned int *count)
{
  unsigned int n=0U;
  unsigned char *p=(unsigned char *)s;
  while(*p){
    while(*p==' '||*p=='\t'||*p=='\n'||*p=='\r')p++;
    if(*p=='#'||*p=='\0')break;
    if(n>=BC_TSV_TOKENS)return BC_RIG_BAD;
    tokens[n++]=(char *)p;
    while(*p && *p!=' ' && *p!='\t' && *p!='\r' && *p!='\n')p++;
    if(*p){*p=0;p++;}
  }
  *count=n;return BC_RIG_OK;
}
/* Detect lines too long so nothing is silently lost. */
static int bc_next(FILE *f,char **tok,unsigned int *n)
{
  unsigned int len;
  for(;;){
    if(!fgets(bc_line,sizeof(bc_line),f))return ferror(f)?BC_RIG_BAD:1;
    len=(unsigned int)strlen(bc_line);
    if(len==BC_TSV_LINE-1U && bc_line[len-1]!='\n' && !feof(f))return BC_RIG_BAD;
    if(bc_split(bc_line,tok,n)!=0)return BC_RIG_BAD;
    if(*n>0U)return BC_RIG_OK;
  }
}
static int bc_read_mat(char **t,unsigned int n,unsigned int *id,BC_RigMat *mat,int rest_format)
{
  unsigned int k,row,col,start,num;
  int value;
  if(n!=13U && !(rest_format && n==20U))return BC_RIG_BAD;
  if(n==13U){
    if(bc_parse_uint(t[0],id)!=0)return BC_RIG_BAD;
    start=1U;num=12U;
  }else{
    /* --rest-matrices: armature id, bone index, parent index, name, 16 entries. */
    if(strcmp(t[0],"0")!=0 ||bc_parse_uint(t[1],id)!=0)return BC_RIG_BAD;
    start=4U;num=16U;
  }
  if(*id>=BC_RIG_MAX_BONES)return BC_RIG_BAD;
  bc_rig_mat_identity(mat);
  for(k=0U;k<num;k++){
    if(bc_parse_q16(t[start+k],&value)!=0)return BC_RIG_BAD;
    row=k/4U;col=k%4U;
    mat->m[row*4U+col]=value;
  }
  if(n==20U && (mat->m[12]!=0 || mat->m[13]!=0 ||
                mat->m[14]!=0 || mat->m[15]!=65536))return BC_RIG_BAD;
  return BC_RIG_OK;
}
static int bc_read_palette(FILE *f,unsigned int *max_bone,int rest_format)
{
  unsigned int n,id,seen=0U;
  int r;BC_RigMat temp;
  *max_bone=0U;
  memset(bc_present,0,sizeof(bc_present));
  for(;;){
    r=bc_next(f,bc_tokens,&n);
    if(r==1)break;
    if(r!=0 ||bc_read_mat(bc_tokens,n,&id,&temp,rest_format)!=0)return BC_RIG_BAD;
    if(bc_present[id])return BC_RIG_BAD;
    bc_palette[id]=temp;
    bc_present[id]=1U;seen++;
    if(id+1U>*max_bone)*max_bone=id+1U;
  }
  return seen?BC_RIG_OK:BC_RIG_BAD;
}
static int bc_process_vertices(FILE *vertices,FILE *output,unsigned int total_bones)
{
  unsigned int n,id,nweights,i,bi,vertices_done=0U;
  int pt[3],dst[3],w,r;
  BC_RigInfluence weights[BC_RIG_MAX_WEIGHTS];
  if(fputs("# vertex_index\tx\ty\tz\n",output)==EOF)return BC_RIG_BAD;
  for(;;){
    r=bc_next(vertices,bc_tokens,&n);
    if(r==1)break;
    if(r!=0 || n<5U || bc_parse_uint(bc_tokens[0],&id)!=0 ||
       bc_parse_q16(bc_tokens[1],&pt[0])!=0 ||
       bc_parse_q16(bc_tokens[2],&pt[1])!=0 ||
       bc_parse_q16(bc_tokens[3],&pt[2])!=0 ||
       bc_parse_uint(bc_tokens[4],&nweights)!=0 ||
       nweights>BC_RIG_MAX_WEIGHTS ||n!=5U+2U*nweights)return BC_RIG_BAD;
    for(i=0U;i<nweights;i++){
      if(bc_parse_uint(bc_tokens[5U+2U*i],&bi)!=0 ||
         bi>=total_bones || !bc_present[bi] ||
         bc_parse_q16(bc_tokens[6U+2U*i],&w)!=0 ||
         w<0 ||w>BC_RIG_ONE)return BC_RIG_BAD;
      weights[i].bone=bi;
      weights[i].weight=w;
    }
    if(bc_rig_skin_point(bc_palette,total_bones,pt,weights,nweights,dst)!=0)return BC_RIG_BAD;
    if(fprintf(output,"%u\t",id)<0)return BC_RIG_BAD;
    for(i=0U;i<3U;i++){
      if(i>0U && fputc('\t',output)==EOF)return BC_RIG_BAD;
      bc_print_q16(output,dst[i]);
    }
    if(fputc('\n',output)==EOF)return BC_RIG_BAD;
    vertices_done++;
  }
  return vertices_done && !ferror(output)?BC_RIG_OK:BC_RIG_BAD;
}
int bc_skin_tsv(const char *palette_path,const char *vertex_path,const char *output_path)
{
  FILE *palette=NULL,*vertices=NULL,*output=NULL;
  unsigned int bone_count=0U;
  int r=BC_RIG_BAD,created=0;
  if(strcmp(palette_path,output_path)==0 || strcmp(vertex_path,output_path)==0)return BC_RIG_BAD;
  palette=fopen(palette_path,"rb");
  if(!palette)goto done;
  vertices=fopen(vertex_path,"rb");
  if(!vertices)goto done;
  if(bc_read_palette(palette,&bone_count,0)!=0)goto done;
  output=fopen(output_path,"wb");
  if(!output)goto done;
  created=1;
  r=bc_process_vertices(vertices,output,bone_count);
 done:
  if(output && fclose(output)!=0)r=BC_RIG_BAD;
  if(vertices)fclose(vertices);
  if(palette)fclose(palette);
  if(created && r!=BC_RIG_OK)remove(output_path);
  return r;
}

/* Compose P * inverse(R) into the standalone deform palette required by
   --skin-tsv. Pose values must already be evaluated in armature space. */
int bc_palette_tsv(const char *rest_path,const char *pose_path,const char *output_path)
{
  FILE *rest=NULL,*pose=NULL,*out=NULL;
  unsigned int bone_count=0U,n,id,k,row,col,seen=0U;
  BC_RigMat pm,deform;
  int r=BC_RIG_BAD,read_status,created=0;
  if(strcmp(rest_path,output_path)==0 || strcmp(pose_path,output_path)==0)return BC_RIG_BAD;
  rest=fopen(rest_path,"rb");if(!rest)goto done;
  pose=fopen(pose_path,"rb");if(!pose)goto done;
  if(bc_read_palette(rest,&bone_count,1)!=0)goto done;
  out=fopen(output_path,"wb");if(!out)goto done;
  created=1;
  if(fputs("# bone_id\tm00..m23 (pose * inverse(rest))\n",out)==EOF)goto done;
  memset(bc_pose_seen,0,sizeof(bc_pose_seen));
  for(;;){
    read_status=bc_next(pose,bc_tokens,&n);
    if(read_status==1)break;
    if(read_status!=0 || n!=13U ||
       bc_read_mat(bc_tokens,n,&id,&pm,0)!=0 || id>=bone_count ||
       !bc_present[id] || bc_pose_seen[id] ||
       bc_rig_deform_matrix(&pm,&bc_palette[id],&deform)!=0)goto done;
    bc_pose_seen[id]=1U;seen++;
    if(fprintf(out,"%u",id)<0)goto done;
    for(k=0U;k<12U;k++){
      row=k/4U;col=k%4U;
      if(fputc('\t',out)==EOF)goto done;
      bc_print_q16(out,deform.m[row*4U+col]);
    }
    if(fputc('\n',out)==EOF)goto done;
  }
  for(id=0U;id<bone_count;id++)if(bc_present[id] && !bc_pose_seen[id])goto done;
  if(seen && !ferror(out))r=BC_RIG_OK;
 done:
  if(out && fclose(out)!=0)r=BC_RIG_BAD;
  if(pose)fclose(pose);
  if(rest)fclose(rest);
  if(created && r!=BC_RIG_OK)remove(output_path);
  return r;
}
