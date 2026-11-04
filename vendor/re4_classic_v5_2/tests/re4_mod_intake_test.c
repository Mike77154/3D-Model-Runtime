#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do{if(!(x)){fprintf(stderr,"mod intake test line %d\n",__LINE__);return 1;}}while(0)
static re4_err parse(const char *t,re4_mod_package *p){re4_span s;s.data=(const re4_u8*)t;s.size=(re4_size)strlen(t);return re4_mod_parse(s,p);}
int main(void)
{
 re4_mod_package p;
 const char *good="R4MOD1\nformat=pmd2007\nmaster=mod/body.pmd\npart=mod/hair.pmd\npart=mod/hand.pmd\nclip=motion/idle.fcv\ntexture=mod/face.tga\n";
 CK(parse(good,&p)==RE4_OK);CK(p.part_count==2U);CK(p.clip_count==1U);CK(p.texture_count==1U);
 CK(strcmp(p.master.path,"mod/body.pmd")==0);
 CK(parse("R4MOD1\nformat=bin-uhd\nmaster=mod\\enemy.bin\ntexture=mod\\tex.tpl\n",&p)==RE4_OK);
 CK(strcmp(p.master.path,"mod/enemy.bin")==0);
 CK(!re4_mod_path_safe("../evil.pmd"));CK(!re4_mod_path_safe("mod/../evil.pmd"));
 CK(!re4_mod_path_safe("C:/evil.pmd"));CK(!re4_mod_path_safe("/evil.pmd"));
 CK(!re4_mod_path_safe("mod//evil.pmd"));CK(!re4_mod_path_safe("foo\\..\\bar.pmd"));
 CK(parse("R4MOD1\nformat=pmd2007\nmaster=bad.bin\n",&p)==RE4_ERR_BAD_FORMAT);
 CK(parse("R4MOD1\nformat=pmd2007\nmaster=x.pmd\npart=x.pmd\n",&p)==RE4_ERR_BAD_FORMAT);
 CK(parse("R4MOD1\nformat=pmd2007\nmaster=x.pmd\npart=dir\\a.pmd\npart=dir/a.pmd\n",&p)==RE4_ERR_BAD_FORMAT);
 CK(parse("R4MOD1\nformat=pmd2007\nmaster=x.pmd\nclip=../../z.fcv\n",&p)==RE4_ERR_BAD_FORMAT);
 CK(parse("R4MOD1\nformat=pmd2007\nmaster=x.pmd\nformat=bin-uhd\n",&p)==RE4_ERR_BAD_FORMAT);
 CK(parse("R4MOD1\nformat=pmd2007\nmaster=x.pmd\nunknown=x\n",&p)==RE4_ERR_BAD_FORMAT);
 CK(parse("R4MOD1\nformat=pmd2007\nmaster=x.pmd\ntexture=other.dds\n",&p)==RE4_ERR_BAD_FORMAT);
 CK(parse("R4MOD1\nformat=bin-uhd\nmaster=x.bin\ntexture=tex.dds\n",&p)==RE4_OK);
 puts("RE4 external mod manifest parser PASS");return 0;
}
