/* Gun Survivor NAC v19: all 31 original animation catalogs incl. no-TC variants.
 * ANSI C89, no malloc/free, no float/double, no stdin.
 * Usage: gsnacdump input.NAC number_of_tracks output_prefix
 * Exports native references and raw key coordinates; NO invented frame rate.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define LIMIT_RECORDS 500000UL
#define LIMIT_EVENTS 800000UL
static unsigned long rd32(FILE *f,int *ok){
 int a,b,c,d;
 a=fgetc(f);b=fgetc(f);c=fgetc(f);d=fgetc(f);
 if(a==EOF||b==EOF||c==EOF||d==EOF){*ok=0;return 0UL;}
 return (unsigned long)a|((unsigned long)b<<8)|((unsigned long)c<<16)|((unsigned long)d<<24);
}
static int seek_u(FILE *f,unsigned long off,unsigned long maxfile){
 if(off>maxfile||off>(unsigned long)LONG_MAX)return 0;
 return fseek(f,(long)off,SEEK_SET)==0;
}
static int integral_f32(unsigned long u,long *out){
 unsigned long exponent=(u>>23)&255UL,mag=u&0x7fffffUL,val;
 int shift;
 if(exponent==255UL)return 0;
 if(exponent==0UL){*out=0;return mag==0UL;}
 mag|=0x800000UL;shift=(int)exponent-150;
 if(shift>=0){
  if(shift>7||mag>((unsigned long)LONG_MAX>>shift))return 0;
  val=mag<<shift;
 }else{
  if(shift<=-32)return 0;
  if((mag&((1UL<<(-shift))-1UL))!=0UL)return 0;
  val=mag>>(-shift);
 }
 if(val>(unsigned long)LONG_MAX)return 0;
 *out=(u&0x80000000UL)?-(long)val:(long)val;
 return 1;
}
static int get_evt(FILE *f,unsigned long base,unsigned long ix,unsigned long n,unsigned long lim,
                   unsigned long *w,unsigned long *v,int *ok){
 unsigned long off;
 if(ix>=n||ix>(ULONG_MAX-base)/8UL)return 0;
 off=base+ix*8UL;
 if(off>lim||lim-off<8UL||!seek_u(f,off,lim))return 0;
 *w=rd32(f,ok);*v=rd32(f,ok);
 return *ok;
}
int main(int argc,char **argv){
 FILE *in,*out,*clips;
 unsigned long records,event_count,base,limit,first,second,tracks,clip_count;
 unsigned long t,c,at,keys=0UL,groups=0UL,tail;
 unsigned long opener,tag,evt,record,tag2=0UL,ref2,k,rec_words[28];
 unsigned long tstart,firstrec,lastrec,nkey;
 int ok=1,tc;
 char name[1024],*end;
 if(argc!=4){fprintf(stderr,"Usage: gsnacdump file.NAC ntracks out_prefix\n");return 2;}
 if(strlen(argv[3])>900UL)return 2;
 tracks=strtoul(argv[2],&end,10);
 if(*end||tracks==0UL||tracks>256UL)return 2;
 in=fopen(argv[1],"rb");if(!in){perror("NAC");return 2;}
 if(fseek(in,0L,SEEK_END)!=0)goto fail;
 {long l=ftell(in);if(l<0L){goto fail;}limit=(unsigned long)l;}
 if(!seek_u(in,0UL,limit))goto fail;
 records=rd32(in,&ok);
 if(!ok||records==0UL||records>LIMIT_RECORDS||records>(limit-4UL)/112UL)goto fail;
 base=4UL+records*112UL;
 if(!seek_u(in,base,limit)||limit-base<12UL)goto fail;
 event_count=rd32(in,&ok);
 if(!ok||event_count==0UL||event_count>LIMIT_EVENTS||event_count>(limit-base-4UL)/8UL||base+4UL+event_count*8UL!=limit)goto fail;
 base+=4UL;
 if(!get_evt(in,base,0UL,event_count,limit,&first,&second,&ok))goto fail;
 tc=((first&65535UL)==0x4354UL);
 if(tc){
  clip_count=(first>>16)&255UL;
  if(((first>>24)&255UL)!=tracks||second!=records-1UL||clip_count==0UL)goto fail;
  at=1UL;
 }else{
  clip_count=0UL;at=0UL;
  for(k=0UL;k<event_count;k++){
   if(!get_evt(in,base,k,event_count,limit,&opener,&tag,&ok))goto fail;
   if(tag==0xffffffffUL&&(opener>>16)==0x8000UL)clip_count++;
  }
  if(clip_count==0UL||clip_count%tracks!=0UL)goto fail;
  clip_count/=tracks;
 }
 sprintf(name,"%s_clips.csv",argv[3]);clips=fopen(name,"wb");if(!clips)goto fail;
 sprintf(name,"%s_keys.csv",argv[3]);out=fopen(name,"wb");if(!out){fclose(clips);goto fail;}
 fputs("clip,track,start_event,end_event,keys,first_record,last_record,close_tag\n",clips);
 fputs("clip,track,key,record,tag_low,opcode,raw_control,scale_x,scale_y,scale_z,rot_x,rot_y,rot_z,trans_x,trans_y,trans_z,fields_integral,raw_key_status\n",out);
 for(t=1UL;t<=tracks;t++){
  for(c=1UL;c<=clip_count;c++){
   if(!get_evt(in,base,at,event_count,limit,&opener,&tag,&ok)||tag!=0xffffffffUL||(opener>>16)!=0x8000UL)goto fail_open;
   tstart=at;at++;nkey=0UL;firstrec=0UL;lastrec=0UL;
   for(;;){
    if(!get_evt(in,base,at,event_count,limit,&evt,&record,&ok))goto fail_open;
    if(record==0xffffffffUL)break;
    if(record>=records)goto fail_open;
    if(nkey==0UL)firstrec=record;
    lastrec=record;
    if(!seek_u(in,4UL+record*112UL,limit))goto fail_open;
    for(k=0UL;k<28UL;k++)rec_words[k]=rd32(in,&ok);
    if(!ok)goto fail_open;
    {long field[9];int valid=1;
     unsigned long ofs[9]={1UL,2UL,3UL,5UL,6UL,7UL,9UL,10UL,11UL};
     for(k=0UL;k<9UL;k++)if(!integral_f32(rec_words[ofs[k]],&field[k])){valid=0;field[k]=0L;}
     fprintf(out,"%lu,%lu,%lu,%lu,%lu,%lu,0x%08lX",c,t,nkey,record,evt&65535UL,evt>>16,evt);
     for(k=0UL;k<9UL;k++)fprintf(out,",%ld",field[k]);
     fprintf(out,",%d,0x%08lX\n",valid,rec_words[0]);
    }
    keys++;nkey++;at++;
   }
   tag2=evt;ref2=record;
   if(ref2!=0xffffffffUL||(tag2>>16)!=0x8080UL+(c-1UL)*0x80UL||(tag2&65535UL)!=(tc?tstart:tstart+1UL))goto fail_open;
   fprintf(clips,"%lu,%lu,%lu,%lu,%lu,%lu,%lu,0x%08lX\n",c,t,tstart,at,nkey,firstrec,lastrec,tag2);
   at++;groups++;
  }
 }
 tail=tc?tracks*10UL:tracks;
 if(at+tail!=event_count)goto fail_open;
 for(k=at;k<event_count;k++){
  if(!get_evt(in,base,k,event_count,limit,&evt,&record,&ok)||record>=records)goto fail_open;
 }
 fclose(clips);fclose(out);fclose(in);
 printf("PASS %s clips=%lu tracks=%lu groups=%lu key_references=%lu records=%lu trailer=%lu\n",tc?"TC":"NO_TC",clip_count,tracks,groups,keys,records,tail);
 return 0;
fail_open:
 fprintf(stderr,"NAC debug: track=%lu clip=%lu at=%lu evt=%08lX record=%08lX tag=%08lX tc=%d\n",t,c,at,evt,record,tag2,tc);
 fclose(out);fclose(clips);
fail:
 fclose(in);
 fputs("FAIL: invalid NAC data; partial CSV may exist\n",stderr);
 return 1;
}
