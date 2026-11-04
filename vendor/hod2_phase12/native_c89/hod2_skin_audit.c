/* HOD2 Phase10, CharacterDrawBonePart EXE <-> POL linkage audit.
 * Strict ANSI C89. No heap, floating point, stdin, external libraries.
 * This is a real-executable binding audit, NOT an animation/skin renderer.
 * Based on verified executable vertex-ref tables described in hotd2-re.
 */
#include "hod2_native.h"
#include "character_draw_bone_part.h"
#include "../inherited/c89/hod2_extra_parts.h"
#include <string.h>

#define SKIN_MAX_REFS 1000000UL
static unsigned long sk_refs[SKIN_MAX_REFS];
static unsigned long sk_count;
static const unsigned char *sk_bytes;
static size_t sk_size;
static int sk_overflow;

static int sk_collect(const unsigned char *vertex,unsigned long tex,void *ctx){
 size_t n;
 (void)tex;(void)ctx;
 if(vertex<sk_bytes||vertex>sk_bytes+sk_size||sk_count>=SKIN_MAX_REFS){sk_overflow=1;return 0;}
 n=(size_t)(vertex-sk_bytes);
 if(n>sk_size||sk_size-n<32U){sk_overflow=1;return 0;}
 sk_refs[sk_count++]=(unsigned long)n;
 return 1;
}
/* In-place heapsort: unlike libc qsort, it cannot allocate a heap buffer. */
static void sk_heap_down(size_t size,size_t parent){
 size_t left,right,largest;unsigned long swap;
 for(;;){
  left=parent*2U+1U;right=left+1U;largest=parent;
  if(left<size&&sk_refs[left]>sk_refs[largest])largest=left;
  if(right<size&&sk_refs[right]>sk_refs[largest])largest=right;
  if(largest==parent)break;
  swap=sk_refs[parent];sk_refs[parent]=sk_refs[largest];sk_refs[largest]=swap;
  parent=largest;
 }
}
static void sk_sort(void){
 size_t i,n=(size_t)sk_count;unsigned long swap;
 for(i=n/2U;i>0U;){--i;sk_heap_down(n,i);}
 for(i=n;i>1U;){--i;swap=sk_refs[i];sk_refs[i]=sk_refs[0];sk_refs[0]=swap;sk_heap_down(i,0);}
}
static int sk_present(unsigned long off){
 unsigned long lo=0UL,hi=sk_count,mid;
 while(lo<hi){mid=lo+(hi-lo)/2UL;if(sk_refs[mid]<off)lo=mid+1UL;else hi=mid;}
 return lo<sk_count&&sk_refs[lo]==off;
}
static int sk_path(char *dst,const char *dir,const char *name){
 size_t a,b,i;
 if(!dir||!name)return 0;
 a=strlen(dir);b=strlen(name);
 if(a+b+2U>=HOD2_MAX_PATH||b<5U)return 0;
 for(i=0U;i<b;i++)if(name[i]=='/'||name[i]=='\\'||name[i]==':')return 0;
 if(strstr(name,".."))return 0;
 memcpy(dst,dir,a);if(a&&dir[a-1]!='/'&&dir[a-1]!='\\')dst[a++]='/';
 memcpy(dst+a,name,b+1U);return 1;
}
/* Compare vertex-reference positions under both documented base candidates.
 * Each reference is a byte offset relative to the loaded POL model pointer. */
static int sk_matches(const Hod2PE *pe,const Hod2ExtraPart *part,
                      unsigned long block,unsigned long root,
                      unsigned long *total,unsigned long *hits_block,unsigned long *hits_root){
 unsigned long r,k,o,va;const unsigned char *p;
 *total=0UL;*hits_block=0UL;*hits_root=0UL;
 if(part->row_stride==0UL||part->row_ptr_count>64UL)return 0;
 for(r=0UL;r<part->row_count;r++){
  va=part->rows_va+r*part->row_stride+part->row_header_bytes;
  if(!hod2_pe_data(pe,va,(size_t)part->row_ptr_count*4U,&p))return 0;
  for(k=0UL;k<part->row_ptr_count;k++){
   o=hod2_u32(p+k*4UL);if(o==0xffffffffUL)break;
   ++*total;
   if(o<=0xffffffffUL-block && sk_present(block+o))++*hits_block;
   if(o<=0xffffffffUL-root && sk_present(root+o))++*hits_root;
  }
 }
 return 1;
}
static void sk_row(FILE *f,unsigned int type,unsigned int pi,const Hod2ExtraPart *part,
                   const char *status,const char *bank,unsigned long total,
                   unsigned long block,unsigned long root,const char *selected){
 fprintf(f,"%u,%u,%lu,%d,%ld,%lu,%lu,%lu,%lu,%lu,%lu,%s,%s,%s\n",
 type,pi,part->slot,part->draw_mode,part->draw_bone,part->row_count,
 total,block,root,part->source_vertex_count[0],part->source_vertex_count[2],
 selected,status,bank?bank:"");
}
int hod2_skin_audit(const Hod2PE *pe,const char *pol_dir,const char *csv,
                    unsigned long *matched,unsigned long *total,unsigned long *parts){
 FILE *out;Hod2ExtraExe ex;Hod2ExtraPart part;Hod2Bank bank;Hod2PolyStat stats;
 unsigned long t,pi,nrefs,hb,hr,block,root,end,chosen,nmatched=0UL,ntotal=0UL,nparts=0UL;
 unsigned int n,inside;int rc,ok;
 size_t size,rawsize;char path[HOD2_MAX_PATH];const char *status,*selected;
 unsigned int mcount;
 if(!pe||!pol_dir||!csv||!matched||!total||!parts)return 0;
 out=fopen(csv,"wb");if(!out)return 0;
 if(fputs("type,part,model_id,draw_mode,draw_bone,logical_rows,source_offsets,matched_block,matched_root,group0,group2,selected_base,status,bank\n",out)==EOF){fclose(out);return 0;}
 hod2ex_init(&ex,pe->file.data,pe->file.size,pe->profile);
 for(t=0UL;t<86UL;t++){
  rc=hod2ex_count(&ex,(unsigned int)t,&n);
  if(rc!=1){memset(&part,0,sizeof(part));sk_row(out,(unsigned int)t,0U,&part,"bad_count","",0UL,0UL,0UL,"none");continue;}
  for(pi=0UL;pi<(unsigned long)n;pi++){
   ++nparts;memset(&part,0,sizeof(part));
   rc=hod2ex_read(&ex,(unsigned int)t,(unsigned int)pi,&part);
   if(rc<0){sk_row(out,(unsigned int)t,(unsigned int)pi,&part,"invalid_descriptor","",0UL,0UL,0UL,"none");continue;}
   if(rc==0){sk_row(out,(unsigned int)t,(unsigned int)pi,&part,"null","",0UL,0UL,0UL,"none");continue;}
   if(part.draw_mode==HOD2EX_DRAW_UNSUPPORTED){sk_row(out,(unsigned int)t,(unsigned int)pi,&part,"unsupported_drawer","",0UL,0UL,0UL,"none");continue;}
   if(!hod2_find_model(pe,(unsigned int)part.slot,&bank,&inside)||
      !sk_path(path,pol_dir,bank.name)||
      !hod2_read_file(path,hod2_workspace_raw,HOD2_FILE_CAP,&rawsize)||
      !hod2_file_decode(hod2_workspace_raw,rawsize,hod2_workspace_decoded,HOD2_FILE_CAP,&size)||
      !hod2_pol_model(hod2_workspace_decoded,size,inside,&root,&end,&mcount)){
    sk_row(out,(unsigned int)t,(unsigned int)pi,&part,"missing_pol","",0UL,0UL,0UL,"none");continue;
   }
   block=hod2_u32(hod2_workspace_decoded+inside*8UL)&0x7fffffffUL;
   sk_count=0UL;sk_overflow=0;sk_bytes=hod2_workspace_decoded;sk_size=size;
   ok=hod2_pol_scan(hod2_workspace_decoded,size,root,end,&stats,sk_collect,(void*)0);
   if(!ok||sk_overflow){sk_row(out,(unsigned int)t,(unsigned int)pi,&part,"unsupported_pol",bank.name,0UL,0UL,0UL,"none");continue;}
   sk_sort();
   if(!sk_matches(pe,&part,block,root,&nrefs,&hb,&hr)){
    sk_row(out,(unsigned int)t,(unsigned int)pi,&part,"bad_reference_table",bank.name,0UL,0UL,0UL,"none");continue;
   }
   if(hb>=hr){chosen=hb;selected="block";}else{chosen=hr;selected="root";}
   status=nrefs==0UL?"empty_refs":(chosen==nrefs?"full_match":"partial_match");
   sk_row(out,(unsigned int)t,(unsigned int)pi,&part,status,bank.name,nrefs,hb,hr,selected);
   nmatched+=chosen;ntotal+=nrefs;
  }
 }
 if(fclose(out)!=0)return 0;
 *matched=nmatched;*total=ntotal;*parts=nparts;return 1;
}
/* A reproducible per-vertex map for the original (unanimated) EXE source.
 * Source float32 values are exported as their original hexadecimal words.
 * A "skin override" is the source bind-pose vector, not an animated frame. */
int hod2_skin_map(const Hod2PE *pe,const char *pol_dir,unsigned int type,
                  unsigned int pi,const char *csv,unsigned long *nref,
                  unsigned long *overrides){
 Hod2ExtraExe ex;Hod2ExtraPart part;Hod2Bank bank;Hod2PolyStat stats;
 CharacterDrawBonePartResult r;
 FILE *f;size_t rawsize,size;char path[HOD2_MAX_PATH];
 unsigned int n,inside,mcount,k;unsigned long root,end,block,total,hb,hr,base,ref,off,va,row,hit=0UL,override=0UL;
 const unsigned char *p,*original;
 int rc;
 if(!pe||!pol_dir||!csv||!nref||!overrides||type>=86U)return 0;
 hod2ex_init(&ex,pe->file.data,pe->file.size,pe->profile);
 if(hod2ex_count(&ex,type,&n)!=1||pi>=n||hod2ex_read(&ex,type,pi,&part)!=1||
    part.draw_mode==HOD2EX_DRAW_UNSUPPORTED||
    !hod2_find_model(pe,(unsigned int)part.slot,&bank,&inside)||
    !sk_path(path,pol_dir,bank.name)||
    !hod2_read_file(path,hod2_workspace_raw,HOD2_FILE_CAP,&rawsize)||
    !hod2_file_decode(hod2_workspace_raw,rawsize,hod2_workspace_decoded,HOD2_FILE_CAP,&size)||
    !hod2_pol_model(hod2_workspace_decoded,size,inside,&root,&end,&mcount))return 0;
 block=hod2_u32(hod2_workspace_decoded+inside*8UL)&0x7fffffffUL;
 sk_count=0UL;sk_overflow=0;sk_bytes=hod2_workspace_decoded;sk_size=size;
 if(!hod2_pol_scan(hod2_workspace_decoded,size,root,end,&stats,sk_collect,(void*)0)||sk_overflow)return 0;
 sk_sort();
 if(!sk_matches(pe,&part,block,root,&total,&hb,&hr)||!total|| (hb<total&&hr<total))return 0;
 base=hb>=hr?block:root;
 f=fopen(csv,"wb");if(!f)return 0;
 if(fputs("type,part,logical_row,reference,model_id,vertex_offset,draw_bone,override,px_bits,py_bits,pz_bits,nx_bits,ny_bits,nz_bits\n",f)==EOF){fclose(f);return 0;}
 for(row=0UL;row<part.row_count;row++){
  va=part.rows_va+row*part.row_stride+part.row_header_bytes;
  if(!hod2_pe_data(pe,va,(size_t)part.row_ptr_count*4U,&p)){fclose(f);return 0;}
  for(k=0U;k<part.row_ptr_count;k++){
   off=hod2_u32(p+4UL*k);if(off==0xffffffffUL)break;
   if(base>0xffffffffUL-off){fclose(f);return 0;}
   ref=base+off;
   if(ref+32UL>end||!sk_present(ref)){fclose(f);return 0;}
   original=hod2_workspace_decoded+ref;
   rc=CharacterDrawBonePart(&ex,&part,off,original,&r);
   if(rc<0){fclose(f);return 0;}
   if(r.original_vertex==0)override++;
   hit++;
   if(fprintf(f,"%u,%u,%lu,%u,%lu,%lu,%ld,%d,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx\n",
      type,pi,row,k,part.slot,ref,r.bone,!r.original_vertex,
      r.xyz_nxyz[0],r.xyz_nxyz[1],r.xyz_nxyz[2],r.xyz_nxyz[3],r.xyz_nxyz[4],r.xyz_nxyz[5])<0){fclose(f);return 0;}
  }
 }
 if(fclose(f)!=0||hit!=total)return 0;
 *nref=hit;*overrides=override;return 1;
}
/* Skeleton kind 23 has a second DRAWER using tail segments, not a POL skin
 * record. Unlike the torso attachment, this is not a regular vertex group. */
int hod2_tail23_report(const Hod2PE *pe,const char *csv,unsigned long *found){
 FILE *f;unsigned long base,tab,arr,def,segs,count,node,model;
 Hod2Bank bank;unsigned int within,nsk,i;Hod2Node nodes[HOD2_MAX_NODES];
 if(!pe||!csv||!found)return 0;
 base=hod2_pe_shift(pe,0x0052ed08UL,0);
 if(!hod2_pe_u32(pe,base+23UL*4UL,&tab)||
    !hod2_pe_u32(pe,tab+4UL,&arr)||
    !hod2_pe_u32(pe,arr+4UL,&def)||
    !hod2_pe_u32(pe,def+0x10UL,&segs)||
    !hod2_pe_u32(pe,def+0x14UL,&count)||
    count==0UL||count>16UL||!hod2_skeleton(pe,23U,nodes,HOD2_MAX_NODES,&nsk))return 0;
 f=fopen(csv,"wb");if(!f)return 0;
 if(fputs("segment,node,model_id,bank_name,bank_model_index,status\n",f)==EOF){fclose(f);return 0;}
 *found=0UL;
 for(i=0U;i<count;i++){
  if(!hod2_pe_u32(pe,segs+16UL*i,&node)||
     !hod2_pe_u32(pe,segs+16UL*i+4UL,&model)){fclose(f);return 0;}
  if(node>=nsk||!hod2_find_model(pe,(unsigned int)model,&bank,&within)){
   fprintf(f,"%u,%lu,%lu,,-1,invalid_reference\n",i,node,model);
   continue;
  }
  fprintf(f,"%u,%lu,%lu,%s,%u,resolved\n",i,node,model,bank.name,within);
  ++*found;
 }
 if(fclose(f)!=0)return 0;
 return 1;
}
