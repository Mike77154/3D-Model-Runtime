/* SDK-grounded UGH header and objectRecord C89 probe.
   No claim that dataStart points to the table in all native exports.
   Reports potential records only after strict bounds checks. */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define MAX_RECORDS 4096UL
#define REC_BYTES 28UL
static unsigned char raw[28];
static unsigned long u32(const unsigned char *s){
 return (unsigned long)s[0] | ((unsigned long)s[1]<<8) |
        ((unsigned long)s[2]<<16) | ((unsigned long)s[3]<<24);
}
static int fetch(FILE *f,long pos,unsigned char *out,size_t n){
 if(fseek(f,pos,SEEK_SET))return 0;
 return fread(out,1,n,f)==n;
}
static int parse(FILE *f,FILE *out){
 long sz,position;unsigned long version,data_start,nobj,count,i;
 unsigned long fields[7],suspicious=0;
 if(fseek(f,0L,SEEK_END))return 0;
 sz=ftell(f);if(sz<16L)return 0;
 if(!fetch(f,0L,raw,16)||memcmp(raw,"ugh0",4))return 0;
 version=u32(raw+4);data_start=u32(raw+8);nobj=u32(raw+12);
 fprintf(out,"kind\tindex\toffset\tvalue\n");
 fprintf(out,"header\t0\t0\tugh0\n");
 fprintf(out,"header_version\t0\t4\t%lu\n",version);
 fprintf(out,"header_dataStart\t0\t8\t%lu\n",data_start);
 fprintf(out,"header_numObjects\t0\t12\t%lu\n",nobj);
 fprintf(out,"file_size\t0\t0\t%ld\n",sz);
 fprintf(out,"status\t0\t0\tSDK_HEADER_FIELDS_CONFIRMED_OBJECT_TABLE_LOCATION_UNVERIFIED\n");
 if(!nobj){fprintf(out,"table_probe\t0\t0\tNO_OBJECTS_DECLARED\n");return 1;}
 if(nobj>MAX_RECORDS || data_start<16UL || data_start>(unsigned long)sz){
  fprintf(out,"table_probe\t0\t0\tNOT_PLAUSIBLE\n");return 1;
 }
 count=((unsigned long)sz-data_start)/REC_BYTES;
 if(count<nobj){fprintf(out,"table_probe\t0\t0\tINSUFFICIENT_BYTES_FOR_CONTIGUOUS_RECORDS\n");return 1;}
 fprintf(out,"table_probe\t0\t%lu\tCANDIDATE_CONTIGUOUS_OBJECT_RECORDS\n",data_start);
 position=(long)data_start;
 for(i=0;i<nobj;i++){
  const char *names[7]={"offset","numAttr","attributes","extraData","firstChild","nextSibling","reserved"};
  int k;
  if(!fetch(f,position,raw,28))return 0;
  for(k=0;k<7;k++){
   fields[k]=u32(raw+4*k);
   fprintf(out,"object_%s\t%lu\t%ld\t%lu\n",names[k],i,position+4*k,fields[k]);
  }
  /* These are only heuristics: the spec does not define index bases or null sentinels here. */
  if(fields[0]>(unsigned long)sz || fields[2]>(unsigned long)sz ||
     fields[3]>(unsigned long)sz)suspicious++;
  position+=(long)REC_BYTES;
 }
 fprintf(out,"table_probe\t0\t0\tPOTENTIAL_POINTERS_OUTSIDE_FILE_%lu\n",suspicious);
 return ferror(out)==0;
}
int main(int argc,char **argv){FILE *f,*out=stdout;int ok;
 if(argc!=2&&argc!=3){fputs("Usage: ughstructures file.ugh [report.tsv]\n",stderr);return 2;}
 f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
 if(argc==3){out=fopen(argv[2],"wb");if(!out){fclose(f);return 2;}}
 ok=parse(f,out);if(out!=stdout)fclose(out);fclose(f);
 if(!ok)fputs("Invalid UGH header or I/O error\n",stderr);
 return ok?0:1;
}
