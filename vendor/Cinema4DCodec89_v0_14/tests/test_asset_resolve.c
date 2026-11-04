#include "c4d89.h"
#include <stdio.h>
#include <string.h>
static const char *paths[]={
 "Group/One/tex/FooHead.jpg",
 "Group/Two/tex/FooHead.jpg",
 "Group/One/tex/FooBody.JPG",
 "Group/Three/Spare.tga",
 "Group/One/tex/extra/Bad.png",
 "Group/One/tex/BODY.TGA"
};
static int get(void *unused,unsigned int i,char *dst,unsigned int max)
{
 size_t n;
 (void)unused;
 if(i>=6u)return 0;
 n=strlen(paths[i]);if(n+1u>max)return 0;
 strcpy(dst,paths[i]);return 1;
}
static int check(const char *scene,const char *name,unsigned int status)
{
 C4D89_ResourceIndex idx;C4D89_ResourceMatch out;int rc;
 idx.get=get;idx.user=NULL;idx.count=6u;
 rc=c4d89_match_bitmap_asset(scene,name,&idx,&out);
 if(rc||out.status!=status){printf("case FAILED: %s %s (got %u rc %d)\n",scene,name,out.status,rc);return 0;}
 return 1;
}
int main(void)
{
 /* Four scenarios: exact local association, unrelated name collision,
    global uniqueness, path traversal / missing. */
 if(!check("Group/One/Hero.c4d","FooHead.jpg",C4D89_ASSET_SCENE_TEX))return 1;
 if(!check("Group\\Two\\Other.c4d","foohead.JPG",C4D89_ASSET_SCENE_TEX))return 1;
 if(!check("Group/X/Empty.c4d","FooHead.jpg",C4D89_ASSET_AMBIGUOUS))return 1;
 if(!check("Group/One/Hero.c4d","FooBody.jpg",C4D89_ASSET_SCENE_TEX))return 1;
 if(!check("Group/One/Hero.c4d","Spare.tga",C4D89_ASSET_UNIQUE_PACKAGE))return 1;
 if(!check("Group/One/Hero.c4d","Missing.png",C4D89_ASSET_NONE))return 1;
 if(!check("../malicious.c4d","FooHead.jpg",C4D89_ASSET_BAD_PATH))return 1;
 if(!check("Group/One/Hero.c4d","../FooHead.jpg",C4D89_ASSET_BAD_PATH))return 1;
 if(!check("Group/Two/Hero.c4d","F:\\old\\path\\FooHead.jpg",C4D89_ASSET_SCENE_TEX))return 1;
 puts("asset resolver: 9/9 checks passed");return 0;
}
