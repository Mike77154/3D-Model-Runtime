#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_mdl mdl;
int main(void){
  static const hl2m_u8 txt[]={ 'V','e','r','t','e','x','L','i','t','G','e','n','e','r','i','c',' ', '{', '\n', '"','$','b','a','s','e','t','e','x','t','u','r','e','"',' ', '"','t','e','s','t','"','\n','}', 0 };
  hl2m_vmt vmt;hl2m_material_fx89 fx;int r;
  if(hl2m_master_guess89("a.dx90.vtx",0,0u)!=HL2M_FORMAT_VTX)return 1;
  if(hl2m_master_guess89("a.VTF",0,0u)!=HL2M_FORMAT_VTF)return 2;
  if(hl2m_master_guess89("a.mdl",0,0u)!=HL2M_FORMAT_MDL)return 3;
  r=hl2m_master_material89(txt,sizeof(txt),&vmt,&fx);
  if(r!=HL2M_OK)return 4;
  if(strcmp(vmt.shader,"VertexLitGeneric"))return 5;
  if(hl2m_master_decode89(HL2M_FORMAT_ANI,txt,sizeof(txt),0,0,&mdl)!=HL2M_ERR_NULL)return 6;
  puts("master: format routing, VMT texturizer and input guards OK");return 0;
}
