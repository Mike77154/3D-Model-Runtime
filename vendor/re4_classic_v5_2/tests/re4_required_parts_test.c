#include "re4c3d/re4_required_parts.h"
#include <stdio.h>
int main(void)
{
 const char *attached[]={"C:\\models\\pl0101.PMD","/tmp/pl01lh00.pmd","/tmp/pl01rh00.pmd"};
 const char *yes[]={"pl0101.pmd","pl01lh00.pmd","pl01rh00.pmd"};
 const char *no[]={"pl0101.pmd","pl01lh00.pmd","pl01rh01.pmd"};
 const char *duplicated[]={"pl0101.pmd","PL0101.PMD"};
 const char *nonbase[]={"other/pl0101.pmd"};
 re4_u32 bad=999U;
 if(!re4_required_parts_check(attached,3U,yes,3U,&bad))return 1;
 if(re4_required_parts_check(attached,3U,no,3U,&bad)||bad!=2U)return 2;
 if(re4_required_parts_check(attached,3U,duplicated,2U,&bad)||bad!=1U)return 3;
 if(re4_required_parts_check(attached,3U,nonbase,1U,&bad)||bad!=0U)return 4;
 if(!re4_required_parts_check(attached,3U,0,0U,&bad))return 5;
 puts("required parts guard OK");return 0;
}
