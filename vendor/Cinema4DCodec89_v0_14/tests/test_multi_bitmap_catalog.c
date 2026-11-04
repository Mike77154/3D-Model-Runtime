#include "c4d89.h"
#include <stdio.h>
#include <string.h>
static C4D89_BitmapCatalog catalog;
static int put_typed_path(FILE *f,const char *name)
{
 const unsigned char param[5]={0x0f,0,0,3,0xe8};
 size_t n=strlen(name),i;
 unsigned int bytes;
 if(n>126u)return 0;
 bytes=(unsigned int)n*2u;
 if(fwrite(param,1u,5u,f)!=5u)return 0;
 if(fputc(0x0f,f)==EOF)return 0;
 if(fputc(0,f)==EOF||fputc(0,f)==EOF||fputc(0,f)==EOF||fputc(0,f)==EOF)return 0;
 if(fputc(0x83,f)==EOF||fputc(0x83,f)==EOF)return 0;
 if(fputc(0,f)==EOF||fputc(0,f)==EOF||fputc(0,f)==EOF||fputc((int)bytes,f)==EOF)return 0;
 for(i=0u;i<n;i++){
  if(fputc(0,f)==EOF||fputc((unsigned char)name[i],f)==EOF)return 0;
 }
 return 1;
}
int main(void)
{
 const char *filename="test_c4d89_multimaps.fixture";
 const unsigned char cls[6]={2,1,0,0,0x16,0x47};
 FILE *f=fopen(filename,"wb");int rc;
 if(!f)return 1;
 if(fwrite("XC4DC4D6",1u,8u,f)!=8u ||
    fwrite(cls,1u,6u,f)!=6u ||
    !put_typed_path(f,"Body.jpg") ||
    !put_typed_path(f,"Body_normal.jpg")){
  fclose(f);remove(filename);return 2;
 }
 if(fclose(f)!=0){remove(filename);return 3;}
 rc=c4d89_scan_material_bitmaps(filename,&catalog);
 remove(filename);
 if(rc||catalog.count!=2u)return 4;
 if(strcmp(catalog.paths[0].name,"Body.jpg") ||
    strcmp(catalog.paths[1].name,"Body_normal.jpg"))return 5;
 if(catalog.paths[0].material_offset!=catalog.paths[1].material_offset)return 6;
 puts("multi bitmap material scanner: 2/2 paths preserved");return 0;
}
