/* Blade3D .xmdl static header recognition; not a complete importer. */
#include "xmdl_vendor.h"
#include <stdio.h>
#include <string.h>
int xmdl_probe(FILE *fp) {
 unsigned char b[64];size_t n,i;int a=-1,v=-1;
 if(fseek(fp,0L,SEEK_SET)!=0)return 0;
 n=fread(b,1,sizeof(b),fp);
 printf("Blade3D XMDL: first %lu bytes:",(unsigned long)n);
 for(i=0;i<n && i<32;i++)printf(" %02X",(unsigned)b[i]);
 printf("\n");
 for(i=0;i+4<=n;i++){
  if(memcmp(b+i,"xmdl",4)==0 && a<0)a=(int)i;
  if(memcmp(b+i,"0100",4)==0 && v<0)v=(int)i;
 }
 if(a>=0)printf("  Found candidate fourcc xmdl at offset %d\n",a);
 if(v>=0)printf("  Found candidate version tag 0100 at offset %d\n",v);
 printf("  Reference DLL compares constants xmdl and 0100 at code VA 0x100057AD / 0x100057BE.\n");
 printf("  The exact on-disk offsets are not proven; no authentic XMDL sample was provided.\n");
 return a>=0 && v>=0; /* probe only, never claim decoded geometry */
}
