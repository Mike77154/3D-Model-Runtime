#include "re4c3d/re4_model.h"
#include <stdio.h>
#include <string.h>
static int expect(const char *in,const char *want,re4_err status)
{
    re4_strref str;char result[256];re4_err e;
    str.ptr=in;str.len=(re4_size)strlen(in);
    e=re4_pmd_texture_key(str,result,sizeof(result));
    if(e!=status || (e==RE4_OK && strcmp(result,want)!=0)){
        fprintf(stderr,"bad key: %s / %d\n",in,(int)e);return 1;
    }
    return 0;
}
int main(void)
{
    int errors=0;char out[5];re4_strref s;
    errors+=expect("LEON_TEX.TGA","leon_tex.tga",RE4_OK);
    errors+=expect("xfile\\pl00\\leon_TEX02.tga","leon_tex02.tga",RE4_OK);
    errors+=expect("/a/../b.TGA","b.tga",RE4_OK);
    errors+=expect("..","",RE4_ERR_BAD_FORMAT);
    errors+=expect("a\\..","",RE4_ERR_BAD_FORMAT);
    errors+=expect("bad\tname.tga","",RE4_ERR_BAD_FORMAT);
    errors+=expect("bad:name.tga","",RE4_ERR_BAD_FORMAT);
    errors+=expect("trailing/","",RE4_ERR_NO_SPACE);
    s.ptr="LEON.TGA";s.len=8;
    if(re4_pmd_texture_key(s,out,sizeof(out))!=RE4_ERR_NO_SPACE)++errors;
    if(errors){fprintf(stderr,"texture name errors=%d\n",errors);return 1;}
    puts("re4_material_name_test PASS");return 0;
}
