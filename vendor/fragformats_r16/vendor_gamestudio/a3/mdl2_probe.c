#include "../gs_registry.h"
#include <string.h>
/* Historical A3 / older MDL2: identification only until proper format corpus. */
int gs_a3_probe(FILE *f,FILE *o,FILE *m,GSStats *s,unsigned long frame){
 unsigned char magic[4];(void)o;(void)frame;
 if(!s || !f || fread(magic,1,4,f)!=4)return 0;
 if(memcmp(magic,"MDL2",4)!=0)return 0;
 memset(s,0,sizeof(*s));s->version=2UL;s->unsupported=1UL;
 if(m)fputs("format\tMDL2\nstatus\tUNSUPPORTED\n",m);
 return 0;
}
