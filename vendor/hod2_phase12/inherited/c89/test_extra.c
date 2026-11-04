#include "hod2_extra_parts.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
 FILE *f;long n;static unsigned char buf[2097152];Hod2ExtraExe e;Hod2ExtraPart p;
 Hod2ExtraVertex v;unsigned int count,i;int t,k,res,profile;
 if(argc!=3){puts("usage: test_extra <1999|2000> <Hod2.exe>");return 2;}
 profile=atoi(argv[1])==1999 ? HOD2EX_VERSION_1999 : HOD2EX_VERSION_2000;
 f=fopen(argv[2],"rb");if(!f)return 2;
 if(fseek(f,0,SEEK_END) || (n=ftell(f))<=0 || fseek(f,0,SEEK_SET)){fclose(f);return 2;}
 if(n>(long)sizeof(buf)){fclose(f);return 2;}
 if(fread(buf,1,(size_t)n,f)!=(size_t)n){fclose(f);return 2;}
 fclose(f);hod2ex_init(&e,buf,(size_t)n,profile);
 for(t=0;t<26;t++){
  res=hod2ex_count(&e,(unsigned int)t,&count);
  if(res<0){printf("FAILED count type=%d\n",t);return 1;}
  printf("TYPE %02d count %u",t,count);
  for(i=0;i<count;i++){
   res=hod2ex_read(&e,(unsigned int)t,i,&p);
   if(res<0){printf(" ERROR part %u\n",i);return 1;}
   if(!res){printf(" NULL");continue;}
   printf(" slot=%04lX mode=%d rows=%lu",p.slot,p.draw_mode,p.row_count);
   if(p.row_count){
    k=hod2ex_vertex(&e,&p,0UL,&v);if(k<0){printf(" ERROR vertex\n");return 1;}
   }
  }
  putchar('\n');
 }
 return 0;
}
