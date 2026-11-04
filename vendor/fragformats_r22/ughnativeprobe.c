/* UGH two-stage stream probe derived from a direct disassembly of the
   fragMOTION 1.3.2 UGH import handler at VA 0x00469FA1.
   No native test-file corpus was available: syntax recognition is not
   interoperability and does not imply animations are decoded.
   Strict ISO C89, no heap, no floating-point arithmetic. */
#include <stdio.h>
#include <string.h>
#include <limits.h>
static unsigned char buf[28];
static unsigned long rd32(const unsigned char *p){
 return (unsigned long)p[0]|((unsigned long)p[1]<<8)|
   ((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
static int fetch(FILE *fp,unsigned long off,unsigned char *b,size_t n,unsigned long size){
 if(off>(unsigned long)LONG_MAX || off>size || n>(size_t)(size-off))return 0;
 if(fseek(fp,(long)off,SEEK_SET)!=0)return 0;
 return fread(b,1,n,fp)==n;
}
static int run(FILE *in,FILE *out){
 long end;unsigned long sz,inner,version,offset,nobj,i;
 const char *recnames[7]={"offset","numAttr","attributes","extraData","firstChild","nextSibling","reserved"};
 if(fseek(in,0L,SEEK_END))return 0;
 end=ftell(in);if(end<16L)return 0;sz=(unsigned long)end;
 if(!fetch(in,0UL,buf,16,sz)||memcmp(buf,"ugh0",4))return 0;
 inner=rd32(buf+4);
 fprintf(out,"kind\toffset\tfield\tvalue\n");
 fprintf(out,"outer\t0\tmagic\tugh0\n");
 fprintf(out,"outer\t4\tinner_header_offset\t%lu\n",inner);
 fprintf(out,"outer\t8\topaque_dword_8\t%lu\n",rd32(buf+8));
 fprintf(out,"outer\t12\topaque_dword_12\t%lu\n",rd32(buf+12));
 fprintf(out,"meta\t0\tfile_size\t%lu\n",sz);
 if(inner<16UL || !fetch(in,inner,buf,16,sz)){
  fprintf(out,"status\t0\tparse\tOUTER_VALID_BUT_NO_INNER_HEADER_AT_POINTER\n");
  return 1;
 }
 version=rd32(buf+4);offset=rd32(buf+8);nobj=rd32(buf+12);
 fprintf(out,"inner\t%lu\tdataID_u32\t%lu\n",inner,rd32(buf));
 fprintf(out,"inner\t%lu\tdataVersion\t%lu\n",inner+4UL,version);
 fprintf(out,"inner\t%lu\tdataStart\t%lu\n",inner+8UL,offset);
 fprintf(out,"inner\t%lu\tnumObjects\t%lu\n",inner+12UL,nobj);
 if(version<0x10000UL||version>0x10001UL){
  fprintf(out,"status\t0\tparse\tVERSION_OUTSIDE_SUPPORTED_1_0_0_TO_1_0_1\n");return 1;
 }
 /* Minimum is 0x00010000 from two WORDs and 0x0046C353. */
 fprintf(out,"status\t0\tversion\tRANGE_00010000_TO_00010001_CONFIRMED_IN_LOADER\n");
 if(offset<16UL || !fetch(in,offset,buf,28,sz)){
  fprintf(out,"status\t0\tparse\tINNER_VALID_BUT_NO_FIRST_OBJECTRECORD\n");return 1;
 }
 for(i=0;i<7UL;i++)fprintf(out,"first_object_candidate\t%lu\t%s\t%lu\n",offset+i*4UL,recnames[i],rd32(buf+i*4UL));
 fprintf(out,"status\t0\tparse\tTWO_STAGE_LAYOUT_FITS_LOADER_READ_SEQUENCE\n");
 fprintf(out,"status\t0\tinteroperability\tNOT_TESTED_WITH_NATIVE_UGH\n");
 return !ferror(out);
}
int main(int argc,char **argv){FILE *in,*out=stdout;int ok;
 if(argc<2||argc>3){fputs("Usage: ughnativeprobe input.ugh [probe.tsv]\n",stderr);return 2;}
 in=fopen(argv[1],"rb");if(!in){perror(argv[1]);return 2;}
 if(argc==3){out=fopen(argv[2],"wb");if(!out){perror(argv[2]);fclose(in);return 2;}}
 ok=run(in,out);
 if(out!=stdout)fclose(out);
 fclose(in);
 if(!ok)fputs("UGH wrapper unreadable or I/O error\n",stderr);
 return ok?0:1;
}
