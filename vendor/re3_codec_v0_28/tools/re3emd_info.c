#include "re3_codec.h"
#include <stdio.h>
int main(int argc,char **argv){
 re3c_model m;char report[256];re3c_u32 i;int r;
 if(argc!=2){fprintf(stderr,"usage: %s file.EMD\n",argv[0]);return 2;}
 re3c_model_init(&m);r=re3c_open_file(&m,argv[1]);
 if(r!=RE3C_OK){fprintf(stderr,"open: %s\n",re3c_result_string(r));return 1;}
 printf("RE3_codec %u.%u.%u\n",RE3C_VERSION_MAJOR,RE3C_VERSION_MINOR,RE3C_VERSION_PATCH);
 printf("sections=%u index=0x%08X body_parts=%u vertices=%u faces=%u aux=%u\n",
  (unsigned)m.section_count,(unsigned)m.index_offset,(unsigned)m.body.part_count,
  (unsigned)m.body.vertex_count,(unsigned)m.body.face_count,(unsigned)m.has_aux_mesh);
 for(i=0;i<m.section_count;i++)if(m.sections[i].present)printf("  %02u %-14s off=0x%08X size=%u\n",(unsigned)i,re3c_section_name(i),(unsigned)m.sections[i].offset,(unsigned)m.sections[i].size);
 for(i=0;i<RE3C_BANK_COUNT;i++)if(m.banks[i].present)printf("bank %u %-20s joints=%u clips=%u keys=%u frames=%u inherited=%u\n",(unsigned)i,re3c_bank_name(i),(unsigned)m.banks[i].joint_count,(unsigned)m.banks[i].clip_count,(unsigned)m.banks[i].key_count,(unsigned)m.banks[i].physical_frame_count,(unsigned)m.banks[i].hierarchy_inherited);
 r=re3c_validate(&m,report,sizeof(report));if(report[0])fputs(report,stdout);re3c_model_close(&m);return r==RE3C_OK?0:1;
}
