/* Inspect Source MDL sequences and compare native cell sampling with the
   fractional master evaluator. C89, no floating point or heap. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CAP (8u*1024u*1024u)
static hl2m_u8 bytes[CAP],ani[CAP];
static hl2m_mdl mdl;
static hl2m_pose_matrices a,b,c;
static unsigned int load(const char *p,hl2m_u8 *dst){FILE *f=fopen(p,"rb");size_t n;
 if(!f)return 0u;
 n=fread(dst,1u,CAP,f);
 if(!feof(f)){fclose(f);return 0u;}
 fclose(f);return (unsigned int)n;}
static unsigned int absval(hl2m_fixed a){return a<0?(unsigned int)(-(a+1))+1u:(unsigned int)a;}
int main(int argc,char **argv){unsigned int sz,anis=0u;int r,fix=0,si=0,indices[4],i,k,n,success=0,fail=0;
 const char *anipath=NULL;const hl2m_mdl_seqdesc *s;
 if(argc<2 || argc>4){puts("usage: probe116 model.mdl [sequence_index] [optional.ani]");return 1;}
 sz=load(argv[1],bytes);if(!sz)return 2;
 if(hl2m_repair_missing_first_byte(bytes,&sz,CAP,HL2M_FORMAT_MDL,&fix))return 3;
 r=hl2m_master_decode89(HL2M_FORMAT_MDL,bytes,sz,0,0,&mdl);
 if(r!=HL2M_OK){printf("decode err=%d\n",r);return 4;}
 if(argc>=3)si=atoi(argv[2]);
 if(argc==4)anipath=argv[3];
 if(anipath){anis=load(anipath,ani);if(!anis)return 5;}
 if(si==-1){int j;for(j=0;j<mdl.sequence_count;j++)printf("SEQ %d %s group=%dx%d flags=%d\n",j,mdl.sequences[j].label,mdl.sequences[j].groupsize[0],mdl.sequences[j].groupsize[1],mdl.sequences[j].flags);return 0;}
 if(si<0 || si>=mdl.sequence_count)return 6;
 s=&mdl.sequences[si];printf("MODEL=%s SEQ=%s idx=%d group=%dx%d flags=%d blend_count=%d\n",
    mdl.name,s->label,si,s->groupsize[0],s->groupsize[1],s->flags,s->numblends);
 for(k=0;k<3;k++){
  hl2m_fixed cycle=k==0?0:(k==1?32768:49152);
  r=hl2m_master_sequence89(bytes,sz,anipath?ani:NULL,anis,&mdl,si,cycle,32768,32768,&a,indices);
  if(r!=HL2M_OK){printf("CYCLE=%d ERR=%d\n",k,r);fail++;continue;}
  n=a.bone_count;printf("CYCLE=%d CELL=%d,%d,%d,%d bones=%d root_x=%ld root_y=%ld\n",k,
   indices[0],indices[1],indices[2],indices[3],n,(long)a.world[0].m[0][3],(long)a.world[0].m[1][3]);
  for(i=0;i<n;i++){
   /* reconstructed axes should be reasonably normalized */
   int col;for(col=0;col<3;col++){
    unsigned int l=absval(a.local[i].m[0][col])+absval(a.local[i].m[1][col])+absval(a.local[i].m[2][col]);
    if(l<25000u || l>118000u){printf("BAD_ROT bone=%d axis=%d sum=%u\n",i,col,l);fail++;}
   }
  }success++;
 }
 if(s->groupsize[0]>1 || s->groupsize[1]>1){
  r=hl2m_master_sequence89(bytes,sz,anipath?ani:NULL,anis,&mdl,si,24576,0,0,&b,NULL);
  if(r!=HL2M_OK){printf("BLEND_00_ERR=%d\n",r);fail++;}
  r=hl2m_master_sequence89(bytes,sz,anipath?ani:NULL,anis,&mdl,si,24576,65536,65536,&c,NULL);
  if(r!=HL2M_OK){printf("BLEND_11_ERR=%d\n",r);fail++;}
  if(!fail){int diff=0;for(i=0;i<b.bone_count;i++)if(b.local[i].m[0][3]!=c.local[i].m[0][3] ||
    b.local[i].m[1][0]!=c.local[i].m[1][0])diff++;
    printf("BLEND_DIFFERENT_BONES=%d\n",diff);
  }
 }
 printf("PASS_SAMPLES=%d FAIL=%d\n",success,fail);
 return fail?7:0;
}
