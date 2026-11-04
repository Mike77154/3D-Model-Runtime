#include "re4c3d/re4_bestiaire.h"
#include <stdio.h>
#include <string.h>
static void row(const re4_beast_profile *p){
 printf("%s,%s,%d,%d,%s\n",p->slug,p->family,p->group,p->policy,p->variant);
}
int main(int argc,char **argv){re4_u32 i;const re4_beast_profile *p;
 if(argc==1 || (argc==2 && strcmp(argv[1],"--all")==0)){
  puts("slug,family,group,policy,variant");
  for(i=0;i<re4_beast_count();++i){row(re4_beast_at(i));}
  return 0;
 }
 if(argc==3 && strcmp(argv[1],"--profile")==0){p=re4_beast_find(argv[2]);if(!p)return 2;row(p);return 0;}
 if(argc==4 && strcmp(argv[1],"--part")==0){int r;p=re4_beast_find(argv[2]);if(!p)return 2;
 r=re4_beast_check_part(p,argv[3]);puts(r==1?"ACCEPT_FAMILY_VARIANT":r==0?"REJECT_MISMATCH":"REVIEW_VARIANT");return r==1?0:r==0?2:3;}
 fputs("Usage: re4_bestiaire_catalog [--all | --profile SLUG | --part SLUG FILE.pmd]\n",stderr);return 2;
}
