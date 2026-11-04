#include "character_draw_bone_part.h"
#include "hod2_native.h"
#include <string.h>
int CharacterDrawBonePart(const Hod2ExtraExe *exe,const Hod2ExtraPart *part,
                          unsigned long source_vertex_offset,
                          const unsigned char *original_vertex32,
                          CharacterDrawBonePartResult *result){
 Hod2ExtraVertex v;int status;unsigned int i;
 if(!exe||!part||!original_vertex32||!result)return -1;
 memset(result,0,sizeof(*result));
 result->bone=part->draw_bone;
 result->original_vertex=1;
 result->supported=(part->draw_mode!=HOD2EX_DRAW_UNSUPPORTED);
 for(i=0U;i<6U;i++)result->xyz_nxyz[i]=hod2_u32(original_vertex32+4U*i);
 if(!result->supported)return 0;
 status=hod2ex_vertex(exe,part,source_vertex_offset,&v);
 if(status<0)return -1;
 if(status==1){
  result->bone=v.bone;
  if(v.has_override){
   result->original_vertex=0;
   for(i=0U;i<6U;i++)result->xyz_nxyz[i]=v.raw_position_normal[i];
  }
 }
 return status;
}
