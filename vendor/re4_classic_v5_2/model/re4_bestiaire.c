#include "re4c3d/re4_bestiaire.h"
#include <string.h>
/* Explicit family labels from observed 2007 xfile/em index and modding
   references. Classification is not a substitute for per-piece metadata. */
static const re4_beast_profile profiles[] = {
 {"garrador", "Garrador", "em1c",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_FAMILY_ONLY,""},
 {"garrador-armored", "Garrador blindado", "em1d",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_FAMILY_ONLY,""},
 {"del-lago", "Del Lago", "em2f",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_FAMILY_ONLY,""},
 {"dr-salvador", "Dr. Salvador", "em16",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_FAMILY_ONLY,""},
 {"super-salvador", "Mega Salvador", "em1f",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_FAMILY_ONLY,""},
 {"verdugo", "Verdugo", "em2c",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_FAMILY_ONLY,""},
 {"jj", "J.J.", "em14",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_VARIANT_GUARD,"review"},
 {"el-gigante-a", "El Gigante A", "em2b",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_VARIANT_GUARD,"gigante-a"},
 {"el-gigante-b", "El Gigante B", "em2b",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_VARIANT_GUARD,"gigante-b"},
 {"u3-complete-candidate", "U-3 (torso, abdomen, pinza)", "em32", RE4_GROUP_MINOR_BOSS,RE4_PROFILE_VARIANT_GUARD,"u3-base"},
 {"saddler-spider-candidate", "Saddler mutado araña", "em31", RE4_GROUP_MINOR_BOSS,RE4_PROFILE_VARIANT_GUARD,"saddler-spider"},
 {"mendez-phase-one-candidate", "Mendez fase 1", "em35", RE4_GROUP_MINOR_BOSS,RE4_PROFILE_VARIANT_GUARD,"mendez-a"},
 {"mendez-phase-two-candidate", "Mendez fase 2", "em35", RE4_GROUP_MINOR_BOSS,RE4_PROFILE_VARIANT_GUARD,"mendez-b"},
 {"krauser-boss-candidate", "Krauser jefe: configuración requiere revisión", "em39", RE4_GROUP_MINOR_BOSS,RE4_PROFILE_VARIANT_GUARD,"review"},
 {"bellas-hermanas", "Bellas Hermanas", "",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_UNKNOWN_ASSETS,""},
 {"bella-isabel-candidate", "Bella Isabel (cuerpo + cabeza documentada)", "em12",RE4_GROUP_MINOR_BOSS,RE4_PROFILE_VARIANT_GUARD,"bella-isabel"},
 {"novistador", "Novistador", "em2d",RE4_GROUP_HUMANOID,RE4_PROFILE_VARIANT_GUARD,"review"},
 {"novistador-sin-alas-a", "Novistador A", "em2d",RE4_GROUP_HUMANOID,RE4_PROFILE_VARIANT_GUARD,"nov-a"},
 {"novistador-sin-alas-b", "Novistador B", "em2d",RE4_GROUP_HUMANOID,RE4_PROFILE_VARIANT_GUARD,"nov-b"},
 {"novistador-alado", "Novistador alado", "em2d",RE4_GROUP_HUMANOID,RE4_PROFILE_VARIANT_GUARD,"nov-winged"},
 {"armadura-a", "Armadura A", "em3c",RE4_GROUP_HUMANOID,RE4_PROFILE_VARIANT_GUARD,"armor-a"},
 {"armadura-b", "Armadura B", "em3c",RE4_GROUP_HUMANOID,RE4_PROFILE_VARIANT_GUARD,"armor-b"},
 {"regenerador", "Regenerador", "em36",RE4_GROUP_HUMANOID,RE4_PROFILE_VARIANT_GUARD,"regen"},
 {"iron-maiden", "Iron Maiden", "em36",RE4_GROUP_HUMANOID,RE4_PROFILE_VARIANT_GUARD,"maiden"},
 {"zealot-acolyte", "Acolito variante castillo", "em19",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"ganado-leon-jacket", "Ganado chamarra Leon", "em44",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"ganado-jose", "Don Jose y campesinos", "em10",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"ganado-maria", "Ganada Maria", "em11",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"ganado-isabel", "Ganada Isabel", "em12",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"ganado-manuel", "Ganado Manuel", "em13",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"ganado-esteban", "Ganado Esteban", "em15",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"zealot-black", "Iluminado negro", "em19",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"zealot-red", "Iluminado rojo", "em1a",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"zealot-blue", "Iluminado azul", "em1b",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"island-soldier", "Soldado isla", "em1e",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"island-hammer", "Soldado martillo", "em20",RE4_GROUP_GANADO,RE4_PROFILE_FAMILY_ONLY,""},
 {"military-location", "Paquetes soldados de sala", "em40",RE4_GROUP_GANADO,RE4_PROFILE_VARIANT_GUARD,"review"},
 {"plaga-a", "Plaga tipo A", "",RE4_GROUP_PARASITE,RE4_PROFILE_UNKNOWN_ASSETS,""},
 {"plaga-b", "Plaga tipo B", "",RE4_GROUP_PARASITE,RE4_PROFILE_UNKNOWN_ASSETS,""},
 {"plaga-c", "Plaga tipo C (origen no confirmado)", "",RE4_GROUP_PARASITE,RE4_PROFILE_UNKNOWN_ASSETS,""},
 {"plaga-arana", "Plaga Araña locomotora", "em25",RE4_GROUP_PARASITE,RE4_PROFILE_VARIANT_GUARD,"plaga-arana"},
 {"plaga-cabeza-a", "Plaga de cabeza A (sin PMD confirmado)", "",RE4_GROUP_PARASITE,RE4_PROFILE_UNKNOWN_ASSETS,""},
 {"plaga-cabeza-b", "Plaga de cabeza B (sin PMD confirmado)", "",RE4_GROUP_PARASITE,RE4_PROFILE_UNKNOWN_ASSETS,""},
 {"plaga-cabeza-c", "Plaga de cabeza C (sin PMD confirmado)", "",RE4_GROUP_PARASITE,RE4_PROFILE_UNKNOWN_ASSETS,""},
 {"arana-ambiente", "Araña ambiental, sin FCV", "em2e",RE4_GROUP_PARASITE,RE4_PROFILE_VARIANT_GUARD,"arana-ambiente"},
 {"colmillo", "Colmillo infectado", "em22",RE4_GROUP_PARASITE,RE4_PROFILE_FAMILY_ONLY,""},
 {"cuervo", "Cuervo", "em23",RE4_GROUP_ANIMAL,RE4_PROFILE_FAMILY_ONLY,""},
 {"cuervo-alas-abiertas", "Cuervo con alas abiertas", "em23",RE4_GROUP_ANIMAL,RE4_PROFILE_VARIANT_GUARD,"crow-open"},
 {"cuervo-alas-cerradas", "Cuervo con alas cerradas", "em23",RE4_GROUP_ANIMAL,RE4_PROFILE_VARIANT_GUARD,"crow-closed"},
 {"serpiente", "Serpiente", "em24",RE4_GROUP_ANIMAL,RE4_PROFILE_FAMILY_ONLY,""},
 {"vaca", "Vaca", "em26",RE4_GROUP_ANIMAL,RE4_PROFILE_FAMILY_ONLY,""},
 {"toro", "Toro", "",RE4_GROUP_ANIMAL,RE4_PROFILE_UNKNOWN_ASSETS,""},
 {"pez", "Pez", "em27",RE4_GROUP_ANIMAL,RE4_PROFILE_FAMILY_ONLY,""},
 {"gallina", "Gallina", "em28",RE4_GROUP_ANIMAL,RE4_PROFILE_FAMILY_ONLY,""},
 {"murcielago", "Murcielago", "em29",RE4_GROUP_ANIMAL,RE4_PROFILE_FAMILY_ONLY,""},
 {"perro", "Perro", "em21",RE4_GROUP_ANIMAL,RE4_PROFILE_FAMILY_ONLY,""},
 {"polis-prologo", "Policias del prologo", "pl07",RE4_GROUP_EXTRA,RE4_PROFILE_VARIANT_GUARD,"review"}
};
static int ascii_fold(int c) {if(c>='A' && c<='Z')return c+('a'-'A');return c;}
static int ieq(const char *a,const char *b) {int x,y;if(!a||!b)return 0;
 do{x=ascii_fold((unsigned char)*a++);y=ascii_fold((unsigned char)*b++);if(x!=y)return 0;}while(x);return 1;}
static int prefix(const char *s,const char *p) {int a,b;while(*p){if(!*s)return 0;a=ascii_fold((unsigned char)*s++);b=ascii_fold((unsigned char)*p++);if(a!=b)return 0;}return 1;}
re4_u32 re4_beast_count(void) {return (re4_u32)(sizeof(profiles)/sizeof(profiles[0]));}
const re4_beast_profile *re4_beast_at(re4_u32 i) {return i<re4_beast_count()?&profiles[i]:0;}
const re4_beast_profile *re4_beast_find(const char *slug) {re4_u32 i;
 if(!slug)return 0;
 for(i=0;i<re4_beast_count();++i){
     if(ieq(slug,profiles[i].slug))return &profiles[i];
 }
 return 0;
}
/* File-presence rules: names are derived from observed xfile.dat. Passing
   these rules means a complete *named candidate set*, not a canonical game
   assembly, not a verified IK chain, and not all damage-state variations. */
typedef struct re4_named_set {
 const char *variant;
 re4_u32 size;
 const char *items[8];
} re4_named_set;
static const re4_named_set known_sets[] = {
 {"u3-base",3U,{"em3200.pmd","em3201.pmd","em3202.pmd"}},
 {"saddler-spider",3U,{"em3101.pmd","em3100.pmd","em3102.pmd"}},
 {"mendez-a",2U,{"em3500.pmd","em3501.pmd"}},
 {"mendez-b",2U,{"em3501a.pmd","em3501b.pmd"}},
 {"nov-a",1U,{"em2d00.pmd"}},
 {"nov-b",1U,{"em2d02.pmd"}},
 {"nov-winged",1U,{"em2d04.pmd"}},
 {"regen",8U,{"em3600.pmd","em3601.pmd","em3602.pmd","em3603.pmd","em3604.pmd","em3605.pmd","em3606.pmd","em3607.pmd"}},
 {"maiden",8U,{"em3610.pmd","em3611.pmd","em3612.pmd","em3613.pmd","em3614.pmd","em3615.pmd","em3616.pmd","em3617.pmd"}},
 {"armor-a",2U,{"em3c00.pmd","em3c01.pmd"}},
 {"armor-b",2U,{"em3c00a.pmd","em3c01a.pmd"}},
 {"gigante-a",2U,{"em2b00.pmd","em2b01.pmd"}},
 {"gigante-b",2U,{"em2b00b.pmd","em2b01b.pmd"}},
 {"plaga-arana",1U,{"em2500.pmd"}}
};
static const re4_named_set *get_known_set(const char *variant)
{
 re4_u32 i;
 for(i=0U;i<(re4_u32)(sizeof(known_sets)/sizeof(known_sets[0]));++i) {
  if(ieq(variant,known_sets[i].variant))return &known_sets[i];
 }
 return 0;
}
static const char *base_filename(const char *name)
{const char *r=name,*p;for(p=name;*p;++p)if(*p=='/'||*p=='\\')r=p+1;
 return r;}
static int in_named_set(const re4_named_set *set,const char *name)
{re4_u32 i;for(i=0;i<set->size;++i)if(ieq(name,set->items[i]))return 1;return 0;}
static int ends_pmd(const char *s){size_t n;if(!s)return 0;n=strlen(s);return n>=4U && ieq(s+n-4U,".pmd");}
/* A known family is necessary, but not sufficient for a canonical variant.
   When a filename is not explicitly recognized, return REVIEW (-1). */
int re4_beast_check_part(const re4_beast_profile *p,const char *path) {
 const char *name=path,*it;const char *f;size_t n;const re4_named_set *set;
 if(!p||!path||!p->family||!p->family[0])return 0;
 for(it=path;*it;++it)if(*it=='/'||*it=='\\')name=it+1;
 if(!ends_pmd(name)||!prefix(name,p->family))return 0;
 n=strlen(p->family);f=name+n;
 if(!*f||!(((*f>='0'&&*f<='9')||(*f>='a'&&*f<='z')||(*f>='A'&&*f<='Z'))))return 0;
 if(p->policy==RE4_PROFILE_FAMILY_ONLY)return 1;
 set=get_known_set(p->variant);if(set)return in_named_set(set,name)?1:0;
 if(ieq(p->variant,"crow-open"))return (ieq(name,"em2300.pmd")||ieq(name,"em2301.pmd"))?1:0;
 if(ieq(p->variant,"crow-closed"))return (ieq(name,"em2300.pmd")||ieq(name,"em2302.pmd"))?1:0;
 if(ieq(p->variant,"plaga-arana"))return ieq(name,"em2500.pmd")?1:0;
 if(ieq(p->variant,"arana-ambiente"))return (ieq(name,"em2e00.pmd")||ieq(name,"em2e01.pmd"))?1:0;
 if(ieq(p->variant,"bella-isabel"))return (ieq(name,"em1200.pmd")||ieq(name,"em1203.pmd")||ieq(name,"em12lh00.pmd")||ieq(name,"em12rh00.pmd"))?1:-1;
 if(ieq(p->variant,"regen"))return prefix(name,"em360")?1:0;
 if(ieq(p->variant,"maiden"))return prefix(name,"em361")?1:0;
 if(ieq(p->variant,"armor-a")){
     if(ieq(name,"em3c00.pmd")||ieq(name,"em3c01.pmd"))return 1;
     return -1; /* accessories and weapons require canonical choice */
 }
 if(ieq(p->variant,"armor-b")){
     if(ieq(name,"em3c00a.pmd")||ieq(name,"em3c01a.pmd"))return 1;
     return -1;
 }
 if(ieq(p->variant,"gigante-a")){
     if(ieq(name,"em2b00.pmd")||ieq(name,"em2b01.pmd"))return 1;
     return -1;
 }
 if(ieq(p->variant,"gigante-b")){
     if(ieq(name,"em2b00b.pmd")||ieq(name,"em2b01b.pmd"))return 1;
     return -1;
 }
 return -1;
}
/* Fail-closed whole-assembly guard for the variants verified at file-name
   level. This DOES NOT certify the original game scene or bone retarget. */
int re4_beast_validate_set(const re4_beast_profile *p,
                           const char * const *parts,re4_u32 count)
{
 re4_u32 i;re4_u32 j;re4_u32 mask=0U;int accepted;const re4_named_set *set;
 if(!p||!parts||count==0U||count>32U)return 0;
 if(!p->family||!p->family[0])return 0;
 set=get_known_set(p->variant);
 if(set){
  if(count!=set->size)return 0;
  for(i=0;i<count;++i){
   const char *nm;
   if(!parts[i]||!re4_beast_check_part(p,parts[i]))return 0;
   nm=base_filename(parts[i]);
   if(!in_named_set(set,nm))return 0;
   for(j=0;j<i;++j)if(ieq(nm,base_filename(parts[j])))return 0;
  }
  return 1; /* Named set only. Still requires bone, texture, clip tests. */
 }
 for(i=0;i<count;++i){
  const char *nm;const char *q;
  if(!parts[i])return 0;
  accepted=re4_beast_check_part(p,parts[i]);
  if(accepted==0)return 0;
  if(accepted<0)return -1;
  nm=parts[i];for(q=nm;*q;++q)if(*q=='/'||*q=='\\')nm=q+1;
  if(ieq(p->variant,"crow-open")||ieq(p->variant,"crow-closed")){
   if(ieq(nm,"em2300.pmd")){if(mask&1U)return 0;mask|=1U;}
   else if(ieq(nm,"em2301.pmd")||ieq(nm,"em2302.pmd")){
    if(mask&2U)return 0;
    mask|=2U;
   }
  } else if(ieq(p->variant,"bella-isabel")){
   re4_u32 bit=0U;
   if(ieq(nm,"em1200.pmd"))bit=1U;
   else if(ieq(nm,"em1203.pmd"))bit=2U;
   else if(ieq(nm,"em12lh00.pmd"))bit=4U;
   else if(ieq(nm,"em12rh00.pmd"))bit=8U;
   if(!bit || (mask&bit))return 0;
   mask|=bit;
  } else if(ieq(p->variant,"plaga-arana")||
             ieq(p->variant,"arana-ambiente")){
   if(count>2U)return 0;
  }
 }
 if(ieq(p->variant,"crow-open")||ieq(p->variant,"crow-closed"))
  return (mask==3U&&count==2U)?1:0;
 if(ieq(p->variant,"bella-isabel"))return (mask==15U&&count==4U)?1:0;
 /* Other family-only profiles cannot assert which supplementary meshes
    must be present. Return REVIEW, never a false complete PASS. */
 return -1;
}
