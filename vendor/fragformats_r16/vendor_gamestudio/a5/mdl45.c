#include "../gs_registry.h"
/* A5: MDL4 8/16-bit packed frames and MDL5 variant skins / mipmaps. */
int gs_a5_mdl45(FILE *f,FILE *o,FILE *m,GSStats *s,unsigned long frame){
 return gs_mdl_decode(f,o,m,s,frame);
}
