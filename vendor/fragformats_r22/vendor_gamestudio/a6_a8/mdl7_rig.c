#include "mdl7_rig.h"
#include <string.h>
#include "../../vendor_common/fixed_matrix.h"
#include "../../vendor_common/fixed_quat.h"
/* R10 deliberately opt-in diagnostic, NOT confirmed GameStudio skinning.
 * Raw frame matrices are applied using a row-vector convention, without
 * inverse bind matrices. This is for reproducible parser research only. */
#define GS7_PREVIEW_BONES 4096UL
#define GS7_VERTEX_LIMIT 131072UL
static long gs7_preview_matrices[GS7_PREVIEW_BONES][16];
/* R12 diagnostic hierarchical pose matrices: parent-relative animation. */
static long gs7_world_matrices[GS7_PREVIEW_BONES][16];
/* R15 explicit inverse-bind composition, separate from R12 pivot path. */
static long gs7_skin_matrices[GS7_PREVIEW_BONES][16];
static unsigned char gs7_world_state[GS7_PREVIEW_BONES];
static unsigned short gs7_world_path[GS7_PREVIEW_BONES];
static unsigned char gs7_preview_seen[GS7_PREVIEW_BONES];
static long gs7_preview_acc[GS7_VERTEX_LIMIT][3];
static int gs7_skin_mode=0;
/* R13: opt-in Q16.16 blend of adjacent frame records. These are diagnostic
 * matrix-element blends, NOT quaternion / dual-quaternion interpolation. */
static long gs7_blend_next=0L;
static int gs7_blend_rot=0;
void gs7_set_blend_rot(int enabled){gs7_blend_rot=enabled?1:0;}
static long gs7_bind_snapshot[GS7_VERTEX_LIMIT][3];
static long gs7_next_positions[GS7_VERTEX_LIMIT][3];
static long gs7_selected_mats[GS7_PREVIEW_BONES][16];
static unsigned char gs7_selected_seen[GS7_PREVIEW_BONES];
void gs7_set_blend_next(long factor){
 gs7_blend_next=(factor>=0L && factor<=65536L)?factor:0L;
}

/* Mode 2: MDL7 3x4 column ordering reconstructed from Assimp MDL importer.
 * The animation matrices are treated as local rest-relative transforms.
 * No parent propagation: this is diagnostic, not a native animation player. */
static long gs7_bind_positions[GS7_PREVIEW_BONES][3];
void gs7_set_skin_mode(int mode){gs7_skin_mode=(mode>=0&&mode<=4)?mode:0;}

/* Count influences per vertex without heap allocations. */
static unsigned char gs7_ninfluence[GS7_VERTEX_LIMIT];
/* Weight totals are diagnostics only: some exporters leave unbound vertices. */
static unsigned long gs7_weightsum[GS7_VERTEX_LIMIT];
/* Parents retained to detect circular bone references without heap allocations. */
static unsigned short gs7_parents[65535UL];
static unsigned char gs7_parent_state[65535UL];
/* Build pose transforms in topological order, permitting forward parents.
 * Assumption for this DIAGNOSTIC mode only: A_i is an affine local
 * rest-relative transform; each bone has an absolute rest position B_i.
 * Row-vector convention:
 *   World_i = A_i * Translate(B_i-B_parent) * World_parent
 *   Skinned_i(p) = (p-B_i) * World_i
 * Parent-free root uses Translate(B_root) and identity World_parent.
 * Missing A_i = identity. This is NOT confirmed as native GameStudio math.
 */
static int gs7_build_world(unsigned long n_bones){
 unsigned long i,walk,depth,j;long local[16],translation[16];
 if(n_bones>GS7_PREVIEW_BONES)return 0;
 memset(gs7_world_state,0,(size_t)n_bones);
 for(i=0UL;i<n_bones;i++){
  if(gs7_world_state[i]==2U)continue;
  depth=0UL;walk=i;
  while(walk!=65535UL && gs7_world_state[walk]!=2U){
   if(walk>=n_bones||gs7_world_state[walk]==1U||depth>=n_bones)return 0;
   gs7_world_state[walk]=1U;
   gs7_world_path[depth++]=(unsigned short)walk;
   walk=(unsigned long)gs7_parents[walk];
  }
  while(depth){
   unsigned long bone=(unsigned long)gs7_world_path[--depth];
   unsigned long parent=(unsigned long)gs7_parents[bone];
   fx_identity(translation);
   for(j=0UL;j<3UL;j++)translation[12UL+j]=
    parent==65535UL ? gs7_bind_positions[bone][j] :
     fx_sub(gs7_bind_positions[bone][j],gs7_bind_positions[parent][j]);
   /* First local animation, then rest translation, then parent pose. */
   fx_matrix_mul(gs7_preview_matrices[bone],translation,local);
   if(parent==65535UL)memcpy(gs7_world_matrices[bone],local,sizeof(local));
   else fx_matrix_mul(local,gs7_world_matrices[parent],gs7_world_matrices[bone]);
   gs7_world_state[bone]=2U;
  }
 }
 return 1;
}


static int gs7_valid_float(unsigned long bits){return (bits&0x7f800000UL)!=0x7f800000UL;}
static unsigned long gs7_uint(const unsigned char *p){
 return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
static unsigned gs7_short(const unsigned char *p){return (unsigned)p[0]|((unsigned)p[1]<<8);}
static int gs7_can(FILE *f,unsigned long end,unsigned long n){long p=ftell(f);return p>=0&&(unsigned long)p<=end&&n<=end-(unsigned long)p;}
static int gs7_read(FILE *f,unsigned long end,unsigned char *dst,unsigned long n){
 return gs7_can(f,end,n) && fread(dst,1,(size_t)n,f)==(size_t)n;
}
static int gs7_skip(FILE *f,unsigned long end,unsigned long n){
 return n<=2147483647UL&&gs7_can(f,end,n)&&fseek(f,(long)n,SEEK_CUR)==0;
}
/* Sign, exponent and mantissa decoded by integer shifts; no FP ops. */
static long gs7_fp(unsigned long bits){
 unsigned long exp=(bits>>23)&255UL,m=(bits&0x7fffffUL)|0x800000UL;
 int sh;long r;
 if(exp==0UL || exp==255UL)return 0L;
 sh=(int)exp-134;
 if(sh>7)return (bits&0x80000000UL)?-2147483647L:2147483647L;
 if(sh>=0){if(m>(2147483647UL>>sh))return (bits&0x80000000UL)?-2147483647L:2147483647L;r=(long)(m<<sh);}
 else {if(sh< -31)return 0L;r=(long)(m>>(-sh));}
 return (bits&0x80000000UL)?-r:r;
}
static void gs7_print(FILE *out,long v){
 unsigned long a=(unsigned long)(v<0L?-v:v),i=a/65536UL;
 unsigned long d=((a%65536UL)*10000UL+32768UL)/65536UL;
 if(d>=10000UL){i++;d=0UL;}
 fprintf(out,"%s%lu.%04lu",v<0L?"-":"",i,d);
}
static void gs7_name(char *dst,const unsigned char *src,unsigned n){
 unsigned i;for(i=0;i<n;i++){unsigned char x=src[i];if(x==0U)break;dst[i]=(x<32U||x==127U)?'_':(char)x;}
 dst[i]='\0';
}
int gs7_bones(FILE *f,unsigned long filesize,unsigned long bonecount,unsigned short stride,FILE *meta){
 unsigned long i,j,next;unsigned char rec[36];unsigned parent;char name[21];
 if((bonecount && stride<36U)||bonecount>65535UL)return 0;
 if(gs7_skin_mode){
  if(bonecount>GS7_PREVIEW_BONES)return 0;
  memset(gs7_preview_seen,0,(size_t)bonecount);
  if(gs7_skin_mode>=3 || gs7_blend_next)
   for(i=0UL;i<bonecount;i++)fx_identity(gs7_preview_matrices[i]);
 }
 for(i=0;i<bonecount;i++){
  if(!gs7_read(f,filesize,rec,36UL)||!gs7_skip(f,filesize,(unsigned long)stride-36UL))return 0;
  parent=gs7_short(rec);
  if(parent!=65535U && ((unsigned long)parent>=bonecount || (unsigned long)parent==i))return 0;
  gs7_parents[i]=(unsigned short)parent;
  for(j=0;j<3UL;j++){
   if(!gs7_valid_float(gs7_uint(rec+4+j*4UL)))return 0;
   if(gs7_skin_mode>=2)gs7_bind_positions[i][j]=gs7_fp(gs7_uint(rec+4+j*4UL));
  }
  gs7_name(name,rec+16,20U);
  if(meta){
   fprintf(meta,"bone\t%lu\t%d\t%s\t",i,parent==65535U?-1:(int)parent,name);
   for(j=0;j<3UL;j++){if(j)fputc('\t',meta);gs7_print(meta,gs7_fp(gs7_uint(rec+4+j*4UL)));}
   fputc('\n',meta);
  }
 }
 /* Forward parent references are legal; mark-chain detection is linear. */
 memset(gs7_parent_state,0,(size_t)bonecount);
 for(i=0;i<bonecount;i++){
  if(gs7_parent_state[i])continue;
  next=i;
  while(next!=65535UL && gs7_parent_state[next]==0U){
   gs7_parent_state[next]=1U;next=gs7_parents[next];
  }
  if(next!=65535UL && gs7_parent_state[next]==1U)return 0;
  next=i;
  while(next!=65535UL && gs7_parent_state[next]==1U){
   gs7_parent_state[next]=2U;next=gs7_parents[next];
  }
 }
 return 1;
}
int gs7_deformers(FILE *f,unsigned long end,unsigned long group,unsigned long n_bones,
                  unsigned long n_verts,unsigned char n_deformers,unsigned char max_weights,
                  long (*positions)[3],FILE *meta,GSStats *stats){
 unsigned long d,e,w,nel,nw,group_index,bone,v,weight,blocksize,blockstart;
 unsigned char dh[16],eh[28],wh[8];char name[21];
 if(n_verts>GS7_VERTEX_LIMIT)return 0;
 if(gs7_skin_mode>=3 && !gs7_build_world(n_bones))return 0;
 if(gs7_skin_mode==4){
  /* MDL7 bone XYZ is interpreted as ABSOLUTE bind-space position.
   * R_bind = translate(B_i); skin = inverse(R_bind) * pose_world.
   * Matrix input mode remains a hypothesis; this path verifies inverse-bind
   * construction separately from the older pivot subtraction method. */
  for(bone=0UL;bone<n_bones;bone++){
   long rest[16],inv[16];unsigned long axis;
   fx_identity(rest);
   for(axis=0UL;axis<3UL;axis++)rest[12UL+axis]=gs7_bind_positions[bone][axis];
   /* General affine inverse is independently tested (R16). Bind remains
    * translation-only: GameStudio native bone-local convention unverified. */
   if(!fx_inverse_affine(rest,inv))return 0;
   fx_matrix_mul(inv,gs7_world_matrices[bone],gs7_skin_matrices[bone]);
  }
 }
 memset(gs7_ninfluence,0,(size_t)n_verts);
 if(gs7_skin_mode)memset(gs7_preview_acc,0,(size_t)n_verts*sizeof(gs7_preview_acc[0]));
 memset(gs7_weightsum,0,(size_t)n_verts*sizeof(gs7_weightsum[0]));
 for(d=0;d<(unsigned long)n_deformers;d++){
  blockstart=(unsigned long)ftell(f);
  if(!gs7_read(f,end,dh,16UL))return 0;
  group_index=gs7_uint(dh+4);nel=gs7_uint(dh+8);blocksize=gs7_uint(dh+12);
  if(dh[0]!=0U||dh[1]!=0U||group_index!=group||nel>65535UL||blocksize<16UL||blocksize>end-blockstart){
   stats->unsupported=1UL;return 0;
  }
  for(e=0;e<nel;e++){
   if(!gs7_read(f,blockstart+blocksize,eh,28UL))return 0;
   bone=gs7_uint(eh);nw=gs7_uint(eh+24);
   if(bone>=n_bones||nw>n_verts||nw>(blockstart+blocksize-(unsigned long)ftell(f))/8UL)return 0;
   gs7_name(name,eh+4,20U);
   for(w=0;w<nw;w++){
    if(!gs7_read(f,blockstart+blocksize,wh,8UL))return 0;
    v=gs7_uint(wh);weight=gs7_uint(wh+4);
    if(v>=n_verts || (weight&0x80000000UL)!=0UL || (weight&0x7f800000UL)==0x7f800000UL || (weight&0x7fffffffUL)>0x3f800000UL)return 0;
    if(gs7_ninfluence[v]==255U)return 0;
    if(gs7_weightsum[v] > 2147483647UL - (unsigned long)gs7_fp(weight))return 0;
    gs7_weightsum[v]+=(unsigned long)gs7_fp(weight);
    gs7_ninfluence[v]++;
    if(max_weights && gs7_ninfluence[v]>max_weights)return 0;
    if(gs7_skin_mode && weight!=0UL){
     long temp[3];long wfx=gs7_fp(weight);unsigned long axis;
     if(bone>=GS7_PREVIEW_BONES)return 0;
     if(gs7_preview_seen[bone] || gs7_skin_mode>=3){
      if(gs7_skin_mode>=2){
       long centered[3];
       for(axis=0UL;axis<3UL;axis++)centered[axis]=fx_sub(positions[v][axis],gs7_bind_positions[bone][axis]);
       if(gs7_skin_mode==4)fx_transform_point(gs7_skin_matrices[bone],positions[v],temp);
       else fx_transform_point(gs7_skin_mode==3 ? gs7_world_matrices[bone] : gs7_preview_matrices[bone],centered,temp);
       if(gs7_skin_mode==2)for(axis=0UL;axis<3UL;axis++)temp[axis]=fx_add(temp[axis],gs7_bind_positions[bone][axis]);
      }else fx_transform_point(gs7_preview_matrices[bone],positions[v],temp);
     }else {temp[0]=positions[v][0];temp[1]=positions[v][1];temp[2]=positions[v][2];}
     for(axis=0UL;axis<3UL;axis++){
      gs7_preview_acc[v][axis]=fx_add(gs7_preview_acc[v][axis],fx_mul(temp[axis],wfx));
     }
    }
    stats->influences++;
    if(meta){fprintf(meta,"weight\t%lu\t%lu\t%lu\t%s\t",group,v,bone,name);gs7_print(meta,gs7_fp(weight));fprintf(meta,"\t0x%08lx\n",weight);}
   }
  }
  if((unsigned long)ftell(f)!=blockstart+blocksize)return 0;
  stats->deformers++;
 }
 if(gs7_skin_mode){
  for(d=0;d<n_verts;d++){
   if(gs7_ninfluence[d]){
    unsigned long axis;
    /* Avoid silently translating vertices with incomplete/inflated weights. */
    if(gs7_weightsum[d]<65533UL||gs7_weightsum[d]>65539UL)return 0;
    for(axis=0UL;axis<3UL;axis++)positions[d][axis]=gs7_preview_acc[d][axis];
   }
  }
  if(meta)fprintf(meta,"skin_preview_mode\t%s\n",
   gs7_skin_mode==4?"mdl7_3x4_hierarchy_explicit_inverse_bind_diagnostic":(gs7_skin_mode==3?"mdl7_3x4_hierarchy_local_rest_relative_diagnostic":(gs7_skin_mode==2?"mdl7_3x4_column_pivot_rest_relative_no_parent_propagation":"raw_frame_matrix_row_vector_no_inverse_bind")));
 }
 if(meta){
  unsigned long weighted=0UL,over=0UL;
  for(d=0;d<n_verts;d++){
   if(gs7_ninfluence[d])weighted++;
   if(gs7_weightsum[d]>65536UL)over++;
  }
  fprintf(meta,"weight_summary\t%lu\t%lu\t%lu\t%lu\n",group,weighted,n_verts,over);
 }
 return 1;
}

/* MDL7 frame data: frame header, sparse position overrides, bone matrices.
 * Matrix layout and bind-space convention remain unproven for skinning.
 * Export ALL 16 matrix values (Q16.16) and their IEEE754 raw bits. */
int gs7_frames(FILE *f,unsigned long end,unsigned long count,
               unsigned short hstride,unsigned short vstride,unsigned short mstride,
               unsigned long n_verts,unsigned long n_bones,
               unsigned long selected,long (*positions)[3],
               FILE *meta,GSStats *stats){
 unsigned long i,j,k,nv,nm,idx,bits;
 unsigned char head[24],a[16],b[68];char name[17];
 int selected_or_next;long (*vertex_target)[3];
 if(hstride<24U||vstride<16U||mstride<68U)return 0;
 if(count!=0UL && selected>=count)return 0;
 if(gs7_blend_next){
  if(n_verts>GS7_VERTEX_LIMIT || n_bones>GS7_PREVIEW_BONES)return 0;
  if(count!=0UL && selected+1UL>=count)return 0;
  /* Preserve the unanimated mesh for sparse frame-vertex overrides. */
  for(j=0UL;j<n_verts;j++)for(k=0UL;k<3UL;k++)
    gs7_bind_snapshot[j][k]=positions[j][k];
 }
 for(i=0;i<count;i++){
  if(!gs7_read(f,end,head,24UL)||!gs7_skip(f,end,(unsigned long)hstride-24UL))return 0;
  nv=gs7_uint(head+16);nm=gs7_uint(head+20);
  if(nv>n_verts||nm>n_bones)return 0;
  /* Avoid overflow and reject insufficient bytes BEFORE reading any frames. */
  if(!gs7_can(f,end,0UL))return 0;
  { unsigned long remaining=end-(unsigned long)ftell(f);
    unsigned long vertex_bytes, matrix_bytes;
    if(nv>remaining/(unsigned long)vstride)return 0;
    vertex_bytes=nv*(unsigned long)vstride;remaining-=vertex_bytes;
    if(nm>remaining/(unsigned long)mstride)return 0;
    matrix_bytes=nm*(unsigned long)mstride;
    (void)matrix_bytes;
  }
  gs7_name(name,head,16U);
  selected_or_next=(i==selected || (gs7_blend_next && i==selected+1UL));
  vertex_target=positions;
  if(gs7_blend_next && i==selected+1UL){
   vertex_target=gs7_next_positions;
   for(j=0UL;j<n_verts;j++)for(k=0UL;k<3UL;k++)
     gs7_next_positions[j][k]=gs7_bind_snapshot[j][k];
   if(gs7_skin_mode){
    /* A missing key in the next frame means identity, not previous key. */
    for(j=0UL;j<n_bones;j++)fx_identity(gs7_preview_matrices[j]);
    memset(gs7_preview_seen,0,(size_t)n_bones);
   }
  }
  if(meta)fprintf(meta,"frame\t%lu\t%s\t%lu\t%lu\n",i,name,nv,nm);
  if(i==selected)memcpy(stats->frame_name,name,17U);
  for(j=0;j<nv;j++){
   if(!gs7_read(f,end,a,16UL)||!gs7_skip(f,end,(unsigned long)vstride-16UL))return 0;
   idx=(unsigned long)gs7_short(a+12);
   if(idx>=n_verts)return 0;
   for(k=0;k<3UL;k++)if(!gs7_valid_float(gs7_uint(a+k*4UL)))return 0;
   if(selected_or_next){
    for(k=0;k<3UL;k++)vertex_target[idx][k]=gs7_fp(gs7_uint(a+k*4UL));
    if(meta)fprintf(meta,"pose_vertex\t%lu\t%lu\n",i,idx);
   }
  }
  for(j=0;j<nm;j++){
   if(!gs7_read(f,end,b,68UL)||!gs7_skip(f,end,(unsigned long)mstride-68UL))return 0;
   idx=(unsigned long)gs7_short(b+64);
   if(idx>=n_bones)return 0;
   for(k=0;k<16UL;k++)if(!gs7_valid_float(gs7_uint(b+k*4UL)))return 0;
   if(selected_or_next && gs7_skin_mode){
    if(idx>=GS7_PREVIEW_BONES || gs7_preview_seen[idx])return 0;
    if(gs7_skin_mode>=2){
     /* MDL7 stores a 3x4 affine matrix column-by-column. According to
      * MDLLoader.cpp, [0..3] = a1,b1,c1,d1, [4..7] = a2...d2,
      * [8..11] = a3...d3. Reorder to the internal row-vector layout.
      * [12..15] are ignored in this interpretation. */
     static const unsigned char map[12]={0,4,8,1,5,9,2,6,10,3,7,11};
     unsigned long z;
     fx_identity(gs7_preview_matrices[idx]);
     for(z=0UL;z<3UL;z++){
      gs7_preview_matrices[idx][z]=gs7_fp(gs7_uint(b+map[z]*4U));
      gs7_preview_matrices[idx][4UL+z]=gs7_fp(gs7_uint(b+map[3UL+z]*4U));
      gs7_preview_matrices[idx][8UL+z]=gs7_fp(gs7_uint(b+map[6UL+z]*4U));
      gs7_preview_matrices[idx][12UL+z]=gs7_fp(gs7_uint(b+map[9UL+z]*4U));
     }
    }else{
     for(k=0;k<16UL;k++)gs7_preview_matrices[idx][k]=gs7_fp(gs7_uint(b+k*4UL));
     if(!fx_affine(gs7_preview_matrices[idx]))return 0;
    }
    gs7_preview_seen[idx]=1U;
   }
   if(i==selected && meta){
    fprintf(meta,"pose_bone\t%lu\t%lu\n",i,idx);
    fprintf(meta,"bone_matrix_q16\t%lu\t%lu",i,idx);
    for(k=0;k<16UL;k++){
     fputc('\t',meta);gs7_print(meta,gs7_fp(gs7_uint(b+k*4UL)));
    }
    fputc('\n',meta);
    fprintf(meta,"bone_matrix_raw\t%lu\t%lu",i,idx);
    for(k=0;k<16UL;k++){
     bits=gs7_uint(b+k*4UL);fprintf(meta,"\t%08lx",bits);
    }
    fputc('\n',meta);
    stats->bone_matrices++;
   }
  }
  /* The next-frame branch must not overwrite the chosen pose until after
   * frame parsing completes; keep its matrices distinct as well. */
  if(gs7_blend_next && i==selected && gs7_skin_mode){
   for(j=0UL;j<n_bones;j++){
    memcpy(gs7_selected_mats[j],gs7_preview_matrices[j],sizeof(gs7_selected_mats[j]));
    gs7_selected_seen[j]=gs7_preview_seen[j];
   }
  }
 }
 if(gs7_blend_next && count){
  for(j=0UL;j<n_verts;j++)for(k=0UL;k<3UL;k++)
   positions[j][k]=fx_lerp(positions[j][k],gs7_next_positions[j][k],gs7_blend_next);
  if(gs7_skin_mode){
   for(j=0UL;j<n_bones;j++){
    if(gs7_blend_rot){
     long blended[16];
     if(!fxq_blend_rigid(gs7_selected_mats[j],gs7_preview_matrices[j],gs7_blend_next,blended))return 0;
     for(k=0UL;k<16UL;k++)gs7_preview_matrices[j][k]=blended[k];
    }else{
     for(k=0UL;k<16UL;k++)
      gs7_preview_matrices[j][k]=fx_lerp(gs7_selected_mats[j][k],gs7_preview_matrices[j][k],gs7_blend_next);
    }
    gs7_preview_seen[j]=(unsigned char)(gs7_selected_seen[j] || gs7_preview_seen[j]);
   }
  }
  if(meta){fprintf(meta,"frame_blend_next_q16\t%lu\t%ld\n",selected,gs7_blend_next);
  if(gs7_blend_rot)fputs("frame_blend_rotation\tquaternion_nlerp_shortest_q16_diagnostic\n",meta);}
 }
 return 1;
}
