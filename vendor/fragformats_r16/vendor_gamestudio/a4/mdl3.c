#include "../gs_registry.h"
/* A4: packed 8-bit animation frames. */
int gs_a4_mdl3(FILE *f,FILE *o,FILE *m,GSStats *s,unsigned long frame){
 return gs_mdl_decode(f,o,m,s,frame);
}
