/* v3.6: fail closed unless reviewed ancestry is explicitly requested. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(expr) do{if(!(expr)){fprintf(stderr,"FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    re4_character_link_audit a;memset(&a,0,sizeof(a));
    CK(re4_character_check_policy(0,0U)==RE4_ERR_NULL);
    CK(re4_character_check_policy(&a,0U)!=RE4_OK);
    a.direct=51U;a.unique_direct_targets=51U;a.ancestry_conflicts=12U;
    CK(re4_character_check_policy(&a,RE4_PART_POLICY_STRICT)!=RE4_OK);
    CK(re4_character_check_policy(&a,RE4_PART_POLICY_REVIEW_ANCESTRY)==RE4_OK);
    a.repeated_direct_targets=1U;
    CK(re4_character_check_policy(&a,RE4_PART_POLICY_REVIEW_ANCESTRY)!=RE4_OK);
    a.repeated_direct_targets=0U;a.out_of_range_targets=1U;
    CK(re4_character_check_policy(&a,RE4_PART_POLICY_REVIEW_ANCESTRY)!=RE4_OK);
    a.out_of_range_targets=0U;a.ancestry_conflicts=0U;
    CK(re4_character_check_policy(&a,RE4_PART_POLICY_STRICT)==RE4_OK);
    CK(re4_character_check_policy(&a,2U)==RE4_ERR_RANGE);
    puts("review-only ancestry policy PASS");return 0;
}
