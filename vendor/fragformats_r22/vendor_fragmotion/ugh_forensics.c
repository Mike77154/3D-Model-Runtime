#include "ugh_forensics.h"
#include <string.h>
#include <limits.h>

#define UGH_HEADER 16
#define UGH_STR_MAX 120
#define UGH_STRINGS_LIMIT 256
#define UGH_DIFF_LIMIT 1024

static unsigned long le32(const unsigned char *p) {
 return (unsigned long)p[0] | ((unsigned long)p[1]<<8) |
        ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}
static int filesize(FILE *f,long *length) {
 long n;
 if(fseek(f,0L,SEEK_END)!=0)return 0;
 n=ftell(f);
 if(n<0 || fseek(f,0L,SEEK_SET)!=0)return 0;
 *length=n;return 1;
}
static int is_print(unsigned char c) {return c>=32U && c<=126U;}
static int header(FILE *f, unsigned char b[16],long *length){
 if(!filesize(f,length)||*length<UGH_HEADER)return 0;
 if(fread(b,1,UGH_HEADER,f)!=UGH_HEADER)return 0;
 if(memcmp(b,"ugh0",4)!=0)return 0;
 return 1;
}
/* Only printable candidates. No claim about the real serialization encoding. */
static void ascii_candidates(FILE *f,FILE *report,long length){
 char acc[UGH_STR_MAX+1];long begin=0,pos=UGH_HEADER;unsigned long printed=0;int n=0,c;
 if(fseek(f,UGH_HEADER,SEEK_SET)!=0)return;
 while(pos<length && (c=fgetc(f))!=EOF){
  if(is_print((unsigned char)c)){
   if(n==0)begin=pos;
   if(n<UGH_STR_MAX)acc[n++]=(char)c;
  }else{
   if(n>=4 && printed<UGH_STRINGS_LIMIT){acc[n]=0;fprintf(report,"candidate_ascii\t%ld\t%s\n",begin,acc);printed++;}
   n=0;
  }
  pos++;
 }
 if(n>=4&&printed<UGH_STRINGS_LIMIT){acc[n]=0;fprintf(report,"candidate_ascii\t%ld\t%s\n",begin,acc);}
}
static void utf16_candidates(FILE *f, FILE *report,long length){
 /* Seek to both alignments independently. Distinct from an actual UTF-16 field parser. */
 char acc[UGH_STR_MAX+1];int align;unsigned long printed=0;
 for(align=0;align<2;align++){
  long pos=UGH_HEADER+align,begin=pos;int n=0;
  if(fseek(f,pos,SEEK_SET)!=0)return;
  while(pos+1<length){
   int a=fgetc(f),b=fgetc(f);
   if(a==EOF||b==EOF)break;
   if(b==0 && is_print((unsigned char)a)){
    if(n==0)begin=pos;
    if(n<UGH_STR_MAX)acc[n++]=(char)a;
   }else{
    if(n>=4 && printed<UGH_STRINGS_LIMIT){acc[n]=0;fprintf(report,"candidate_utf16le\t%ld\t%s\n",begin,acc);printed++;}
    n=0;
   }
   pos+=2;
  }
  if(n>=4 && printed<UGH_STRINGS_LIMIT){acc[n]=0;fprintf(report,"candidate_utf16le\t%ld\t%s\n",begin,acc);printed++;}
 }
}
int ugh_inspect(FILE *fp,FILE *report){
 unsigned char b[16],buf[4096];unsigned long hist[256];unsigned long zero=0,printable=0,bytes=0,unique=0;
 long length;size_t n,i;int j;
 if(report==NULL || !header(fp,b,&length))return 0;
 for(j=0;j<256;j++)hist[j]=0;
 fprintf(report,"record\toffset\tvalue\n");
 fprintf(report,"format\t0\tugh0\n");
 fprintf(report,"file_size\t0\t%ld\n",length);
 fprintf(report,"payload_start\t0\t16\n");
 fprintf(report,"payload_size\t16\t%ld\n",length-16);
 fprintf(report,"reserved_word_0\t4\t%08lX\n",le32(b+4));
 fprintf(report,"reserved_word_1\t8\t%08lX\n",le32(b+8));
 fprintf(report,"reserved_word_2\t12\t%08lX\n",le32(b+12));
 /* streamHeader names in SDK, semantics to validate against actual UGH. */
 fprintf(report,"sdk_dataVersion_candidate\t4\t%lu\n",le32(b+4));
 fprintf(report,"sdk_dataStart_candidate\t8\t%lu\n",le32(b+8));
 fprintf(report,"sdk_numObjects_candidate\t12\t%lu\n",le32(b+12));
 fprintf(report,"schema_status\t16\topaque_no_structural_decoder\n");
 while((n=fread(buf,1,sizeof(buf),fp))!=0){
  for(i=0;i<n;i++){
   unsigned char v=buf[i];
   hist[v]++;bytes++;
   if(v==0)zero++;
   if(is_print(v))printable++;
  }
 }
 if(ferror(fp))return 0;
 for(j=0;j<256;j++)if(hist[j])unique++;
 fprintf(report,"payload_bytes_scanned\t16\t%lu\n",bytes);
 fprintf(report,"zero_bytes\t16\t%lu\n",zero);
 fprintf(report,"ascii_printable_bytes\t16\t%lu\n",printable);
 fprintf(report,"unique_byte_values\t16\t%lu\n",unique);
 ascii_candidates(fp,report,length);
 utf16_candidates(fp,report,length);
 return ferror(fp)==0;
}
int ugh_compare(FILE *left,FILE *right,FILE *report){
 unsigned char a[16],b[16];long la,lb,pos=0,st=0;int x,y,active=0;
 unsigned long ranges=0,different=0;int overflow=0;
 if(!header(left,a,&la) || !header(right,b,&lb) || report==NULL)return 0;
 if(fseek(left,0L,SEEK_SET)!=0 || fseek(right,0L,SEEK_SET)!=0)return 0;
 fprintf(report,"type\tstart\tend_exclusive\n");
 fprintf(report,"left_size\t0\t%ld\n",la);
 fprintf(report,"right_size\t0\t%ld\n",lb);
 while(pos<la||pos<lb){
  x=pos<la?fgetc(left):EOF;
  y=pos<lb?fgetc(right):EOF;
  if((pos<la&&x==EOF)||(pos<lb&&y==EOF))return 0;
  if(x!=y){
   if(!active){st=pos;active=1;}
   different++;
  }else if(active){
   if(ranges<UGH_DIFF_LIMIT)fprintf(report,"changed_range\t%ld\t%ld\n",st,pos);
   else overflow=1;
   ranges++;active=0;
  }
  pos++;
 }
 if(active){if(ranges<UGH_DIFF_LIMIT)fprintf(report,"changed_range\t%ld\t%ld\n",st,pos);else overflow=1;ranges++;}
 fprintf(report,"different_bytes\t0\t%lu\n",different);
 fprintf(report,"changed_ranges\t0\t%lu\n",ranges);
 fprintf(report,"truncated_ranges\t0\t%d\n",overflow);
 return 1;
}
