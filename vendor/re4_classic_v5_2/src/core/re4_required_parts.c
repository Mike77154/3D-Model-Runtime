#include "re4c3d/re4_required_parts.h"
static int fold(int c)
{
    if(c>='A' && c<='Z')return c+('a'-'A');
    return c;
}
static const char *base(const char *s)
{
    const char *p=s,*last=s;
    if(!s)return 0;
    while(*p){if(*p=='/' || *p=='\\')last=p+1;++p;}
    return last;
}
static int same(const char *a,const char *b)
{
    if(!a||!b)return 0;
    for(;*a&&*b;++a,++b){if(fold((unsigned char)*a)!=fold((unsigned char)*b))return 0;}
    return !*a && !*b;
}
int re4_required_parts_check(const char * const *attached, re4_u32 attached_count,
                             const char * const *required, re4_u32 required_count,
                             re4_u32 *first_bad)
{
    re4_u32 i,j,found;
    if(first_bad)*first_bad=0U;
    if((attached_count && !attached)||(required_count && !required))return 0;
    for(i=0U;i<required_count;++i){
        const char *wanted=required[i];
        if(!wanted||!wanted[0]||!same(wanted,base(wanted))){if(first_bad)*first_bad=i;return 0;}
        for(j=0U;j<i;++j){if(same(wanted,required[j])){if(first_bad)*first_bad=i;return 0;}}
        found=0U;
        for(j=0U;j<attached_count;++j){
            if(attached[j] && same(base(attached[j]),wanted))++found;
        }
        if(found!=1U){if(first_bad)*first_bad=i;return 0;}
    }
    return 1;
}
