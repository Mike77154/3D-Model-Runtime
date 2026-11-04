#include "../gs_registry.h"
/* A6/A7/A8: MDL7 groups, static triangle geometry; later skinning pending. */
int gs_a678_mdl7(FILE *f,FILE *o,FILE *m,GSStats *s,unsigned long frame){
 return gs_mdl_decode(f,o,m,s,frame);
}
