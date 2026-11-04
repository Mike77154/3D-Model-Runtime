#include "hod2_native.h"
#include "../inherited/c89/hod2_character_clips.h"
#include <string.h>
/* Shared fixed-size motion buffer, no heap, preserves original MOT frame bytes. */
#define mot_data hod2_workspace_raw
static Hod2Node nodes[HOD2_MAX_NODES];
static int path_to(char path[HOD2_MAX_PATH],const char *root,const char *name){
 size_t a,b;if(!root||!name)return 0;a=strlen(root);b=strlen(name);
 if(a+b+2UL>=HOD2_MAX_PATH)return 0;
 memcpy(path,root,a);
 if(a&&root[a-1]!='/'&&root[a-1]!='\\')path[a++]='/';
 memcpy(path+a,name,b+1UL);return 1;
}
static int sample(const Hod2PE *pe,unsigned int mid,unsigned int bones,
                  const unsigned char *raw,size_t size,unsigned long begin,unsigned long end,
                  unsigned long *ticks,unsigned long *stride,unsigned long *frames){
 const unsigned char *p;long t;
 if(mid>=32768U||begin>end||end>(unsigned long)size)return 0;
 if(!hod2_pe_data(pe,hod2_pe_shift(pe,0x004e07d0UL,0)+(unsigned long)mid*2UL,2,&p))return 0;
 t=hod2_s16(p);if(t<=0L)return 0;
 if(!hod2_mot_stride(bones,(unsigned long)t,end-begin,stride,frames))return 0;
 if(end-begin<4UL||begin>size)return 0;
 (void)raw;*ticks=(unsigned long)t;return 1;
}
static int w32(FILE *f,unsigned long w){return fputc((int)(w&255UL),f)!=EOF&&fputc((int)((w>>8)&255UL),f)!=EOF&&fputc((int)((w>>16)&255UL),f)!=EOF&&fputc((int)((w>>24)&255UL),f)!=EOF;}
static int scan_motions(const Hod2PE *pe,const char *dir,unsigned int character,const char *csv,unsigned int want,const char *binary,unsigned long *found){
 unsigned int nodes_count,n,i,cand0=0U,cand1=0U,id0=0U,id1=0U,prov0=0U,prov1=0U;
 unsigned long j,begin,end,ticks,stride,frames,number=0UL;long mid;size_t size;FILE *f=(FILE*)0;Hod2Bank bank;char path[HOD2_MAX_PATH];
 if(!pe||!dir||character>=HOD2_CHARACTER_TYPES||!hod2_skeleton(pe,character,nodes,HOD2_MAX_NODES,&nodes_count))return 0;
 if(csv){f=fopen(csv,"wb");if(!f)return 0;fprintf(f,"bank,motion_id,ticks_60Hz,frames_30Hz,stride,block_bytes,provenance\n");}
 cand0=hod2_character_clip((unsigned int)pe->profile,character,0U,&id0,&prov0);
 cand1=hod2_character_clip((unsigned int)pe->profile,character,1U,&id1,&prov1);
 for(n=0U;n<150U;n++){
  if(!hod2_bank(pe,n,1,&bank))continue;
  if(!path_to(path,dir,bank.name))continue;
  if(!hod2_read_file(path,mot_data,sizeof(mot_data),&size))continue;
  if(size<4UL*(size_t)bank.count)continue;
  for(i=0U;i<bank.count;i++){
   if(!hod2_bank_id(pe,&bank,i,&mid)||mid<0L||mid>32767L)continue;
   if(binary&&(unsigned long)mid!=(unsigned long)want)continue;
   j=(unsigned long)i*4UL;
   begin=hod2_u32(mot_data+j);
   end=(i+1U<bank.count)?hod2_u32(mot_data+j+4UL):(unsigned long)size;
   if(!sample(pe,(unsigned int)mid,nodes_count,mot_data,size,begin,end,&ticks,&stride,&frames))continue;
   number++;
   if(f)fprintf(f,"%s,%ld,%lu,%lu,%lu,%lu,%u\n",bank.name,mid,ticks,frames,stride,end-begin,
                  (cand0&&id0==(unsigned int)mid)?prov0:(cand1&&id1==(unsigned int)mid)?prov1:HOD2_CLIP_BANK_STRIDE_CANDIDATE);
   if(binary){
    FILE *o=fopen(binary,"wb");int ok;
    if(!o){if(f)fclose(f);return 0;}
    ok=(fwrite("H2MC",1,4,o)==4)&&w32(o,1UL)&&w32(o,(unsigned long)pe->profile)&&w32(o,character)&&w32(o,(unsigned long)mid)&&
       w32(o,nodes_count)&&w32(o,ticks)&&w32(o,frames)&&w32(o,stride)&&w32(o,end-begin);
    if(ok)ok=(fwrite(mot_data+(size_t)begin,1,(size_t)(end-begin),o)==(size_t)(end-begin));
    if(fclose(o))ok=0;
    if(!ok)remove(binary);
    if(f)fclose(f);
    if(found)*found=number;
    return ok;
   }
  }
 }
 if(f&&fclose(f))return 0;
 if(found)*found=number;
 return binary?0:1;
}
int hod2_mot_list(const Hod2PE *pe,const char *dir,unsigned int character,const char *csv,unsigned long *n){
 return scan_motions(pe,dir,character,csv,0U,(const char*)0,n);
}
int hod2_mot_dump(const Hod2PE *pe,const char *dir,unsigned int character,unsigned int motion_id,const char *file){
 return scan_motions(pe,dir,character,(const char*)0,motion_id,file,(unsigned long*)0);
}
