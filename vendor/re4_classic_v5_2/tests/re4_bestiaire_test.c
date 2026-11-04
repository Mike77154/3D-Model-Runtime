#include "re4c3d/re4_bestiaire.h"
#include <assert.h>
#include <stdio.h>
int main(void){const re4_beast_profile *p;
 assert(re4_beast_count()>=35U);
 p=re4_beast_find("iron-maiden");assert(p && re4_beast_check_part(p,"XFILE\\EM\\EM36\\em3610.pmd")==1);
 assert(re4_beast_check_part(p,"em3600.pmd")==0);
 p=re4_beast_find("regenerador");assert(p && re4_beast_check_part(p,"em3600.pmd")==1);
 assert(re4_beast_check_part(p,"em3617.pmd")==0);
 p=re4_beast_find("armadura-b");assert(p && re4_beast_check_part(p,"em3c00a.pmd")==1);
 assert(re4_beast_check_part(p,"em3c00.pmd")==0);
 p=re4_beast_find("plaga-a");assert(p && re4_beast_check_part(p,"em2500.pmd")==0);
 p=re4_beast_find("el-gigante-a");assert(p && re4_beast_check_part(p,"em2b01.pmd")==1);
 assert(re4_beast_check_part(p,"em2b00b.pmd")==0);
 p=re4_beast_find("polis-prologo");assert(p && re4_beast_check_part(p,"PL0700.pmd")==-1);
 p=re4_beast_find("cuervo-alas-abiertas");assert(p);
 assert(re4_beast_check_part(p,"em2300.pmd")==1);
 assert(re4_beast_check_part(p,"em2301.pmd")==1);
 assert(re4_beast_check_part(p,"em2302.pmd")==0);
 p=re4_beast_find("cuervo-alas-cerradas");assert(p);
 assert(re4_beast_check_part(p,"em2302.pmd")==1);
 assert(re4_beast_check_part(p,"em2301.pmd")==0);
 p=re4_beast_find("plaga-arana");assert(p);
 assert(re4_beast_check_part(p,"em2500.pmd")==1);
 assert(re4_beast_check_part(p,"em2e00.pmd")==0);
 p=re4_beast_find("plaga-cabeza-c");assert(p);
 assert(re4_beast_check_part(p,"em2500.pmd")==0);
 p=re4_beast_find("bella-isabel-candidate");assert(p);
 assert(re4_beast_check_part(p,"em1203.pmd")==1);
 assert(re4_beast_check_part(p,"em1201.pmd")==-1);
 assert(re4_beast_find("unknown")==0);
 {
  const char *regen_ok[]={"em3600.pmd","em3601.pmd","em3602.pmd","em3603.pmd","em3604.pmd","em3605.pmd","em3606.pmd","em3607.pmd"};
  const char *regen_bad[]={"em3600.pmd","em3601.pmd","em3602.pmd","em3603.pmd","em3604.pmd","em3605.pmd","em3606.pmd","em3617.pmd"};
  const char *u3_ok[]={"em3200.pmd","em3201.pmd","em3202.pmd"};
  const char *armor_ok[]={"em3c00a.pmd","em3c01a.pmd"};
  const char *armor_wrong[]={"em3c00a.pmd","em3c01.pmd"};
  const char *nov_ok[]={"em2d04.pmd"};
  const char *nov_wrong[]={"em2d00.pmd"};
  const char *maiden_one[]={"em3610.pmd"};
  const char *mendez_ok[]={"em3500.pmd","em3501.pmd"};
  p=re4_beast_find("regenerador");
  assert(re4_beast_validate_set(p,regen_ok,8U)==1);
  assert(re4_beast_validate_set(p,regen_bad,8U)==0);
  assert(re4_beast_validate_set(p,regen_ok,7U)==0);
  p=re4_beast_find("u3-complete-candidate");assert(re4_beast_validate_set(p,u3_ok,3U)==1);
  p=re4_beast_find("armadura-b");assert(re4_beast_validate_set(p,armor_ok,2U)==1);
  assert(re4_beast_validate_set(p,armor_wrong,2U)==0);
  p=re4_beast_find("novistador-alado");assert(re4_beast_validate_set(p,nov_ok,1U)==1);
  assert(re4_beast_validate_set(p,nov_wrong,1U)==0);
  p=re4_beast_find("iron-maiden");assert(re4_beast_validate_set(p,maiden_one,1U)==0);
  p=re4_beast_find("mendez-phase-one-candidate");
  assert(re4_beast_validate_set(p,mendez_ok,2U)==1);
 }
 {
  const char *crow_ok[]={"em2300.pmd","em2301.pmd"};
  const char *crow_bad[]={"em2300.pmd","em2302.pmd"};
  const char *bella_ok[]={"em1200.pmd","em1203.pmd","em12lh00.pmd","em12rh00.pmd"};
  const char *bella_missing[]={"em1200.pmd","em1203.pmd","em12lh00.pmd"};
  p=re4_beast_find("cuervo-alas-abiertas");
  assert(re4_beast_validate_set(p,crow_ok,2U)==1);
  assert(re4_beast_validate_set(p,crow_bad,2U)==0);
  assert(re4_beast_validate_set(p,crow_ok,1U)==0);
  p=re4_beast_find("bella-isabel-candidate");
  assert(re4_beast_validate_set(p,bella_ok,4U)==1);
  assert(re4_beast_validate_set(p,bella_missing,3U)==0);
  p=re4_beast_find("plaga-head-a");
  assert(re4_beast_validate_set(p,bella_ok,4U)==0);
 }
 puts("re4_bestiaire_test PASS");return 0;
}
