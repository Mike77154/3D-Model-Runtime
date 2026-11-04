#include "hl2m_master89.h"
#include <stdio.h>
int main(int argc,char **argv){
  int format;
  if(argc!=2){fputs("usage: hl2m_master_probe89 file.ext\n",stderr);return 1;}
  format=hl2m_master_guess89(argv[1],0,0u);
  printf("master ABI=%04x format=%d\n",HL2M_MASTER_ABI89,format);
  return format==HL2M_FORMAT_UNKNOWN;
}
