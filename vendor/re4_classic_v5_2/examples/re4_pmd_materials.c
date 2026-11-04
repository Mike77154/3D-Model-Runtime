/* Strict C89 PMD material inspection CLI. CSV is one record per material.
   Resolving to a TGA is done outside the library by archive index. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define PMDCAP (16UL*1024UL*1024UL)
#define ARENACAP (16UL*1024UL*1024UL)
static re4_u8 raw[PMDCAP],pool[ARENACAP];
static re4_pmd_model pmd;
int main(int argc,char **argv)
{
    FILE *f;long n;re4_span src;re4_arena arena;re4_err err;re4_u32 i;
    char key[256];const re4_pmd_material *m;const char *state;
    if(argc!=2){fputs("usage: re4_pmd_materials file.pmd\n",stderr);return 2;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)!=0 || (n=ftell(f))<58L ||
       (unsigned long)n>PMDCAP || fseek(f,0,SEEK_SET)!=0){fclose(f);return 3;}
    if(fread(raw,1U,(size_t)n,f)!=(size_t)n){fclose(f);return 3;}
    fclose(f);
    src.data=raw;src.size=(re4_size)n;
    re4_arena_init(&arena,pool,ARENACAP);
    err=re4_pmd_read_survey(src,&arena,&pmd);
    if(err!=RE4_OK){fprintf(stderr,"PMD: %s\n",re4_err_name(err));return 4;}
    puts("material_index,texture_enabled,texture_key,state");
    for(i=0U;i<pmd.material_count;++i){
        m=&pmd.materials[i];key[0]='\0';
        if(m->texture_name.len==0U)state="NO_REFERENCE";
        else{
            err=re4_pmd_texture_key(m->texture_name,key,sizeof(key));
            state=err==RE4_OK?"NAMED":(err==RE4_ERR_NO_SPACE?"TOO_LONG":"INVALID_NAME");
        }
        printf("%lu,%ld,%s,%s\n",(unsigned long)i,(long)m->texture_enabled,key,state);
    }
    return 0;
}
