#include "hod2_extra_parts.h"
#include <stdio.h>
#define TEST_FILE_CAP 2000000UL
static unsigned char file_bytes[TEST_FILE_CAP];
int main(int argc,char **argv) {
 FILE *f;size_t n;
 Hod2ExtraExe ex;Hod2ExtraPart p;
 unsigned int typ,count,i,nonempty=0U,skinned=0U,unsupported=0U,empty=0U;
 int ok;
 if(argc!=3)return 2;
 f=fopen(argv[2],"rb");if(!f)return 3;
 n=fread(file_bytes,1U,TEST_FILE_CAP,f);
 if(ferror(f) || !feof(f)){fclose(f);return 4;}
 fclose(f);
 hod2ex_init(&ex,file_bytes,n,argv[1][0]=='1'?HOD2EX_VERSION_1999:HOD2EX_VERSION_2000);
 for(typ=0U;typ<86U;typ++){
  if(hod2ex_count(&ex,typ,&count)!=1)return 10;
  for(i=0U;i<count;i++){
   ok=hod2ex_read(&ex,typ,i,&p);
   if(ok<0){printf("ERROR type=%u index=%u\\n",typ,i);return 11;}
   if(!ok){empty++;continue;}
   nonempty++;
   if(p.draw_mode==HOD2EX_DRAW_UNSUPPORTED)unsupported++;
   else skinned++;
  }
 }
 printf("HOD2 extra-part C89 PASS: nonempty %u skinned %u unsupported %u null %u\n",nonempty,skinned,unsupported,empty);
 if(skinned!=103U || unsupported!=1U || empty!=27U)return 12;
 return 0;
}
