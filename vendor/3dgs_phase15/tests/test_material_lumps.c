#include "../include/gs3d.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)){printf("MATERIAL FAIL line %d\n",__LINE__);return 1;} }while(0)
int main(void){
  GS3D_MaterialInfo mi;GS3D_FileSections s;unsigned long w,h;
  CHECK(gs3d_open("tests/fixtures/mdl7_material_only.mdl")==GS3D_OK);
  CHECK(gs3d_get_material(0U,0U,&mi)==GS3D_OK);
  CHECK(mi.is_explicit && !mi.has_image);
  CHECK(mi.diffuse[0]==32768L && mi.diffuse[1]==16384L && mi.diffuse[2]==49152L);
  CHECK(mi.opacity==57344L);
  CHECK(gs3d_decode_skin(0U,0U,&w,&h)==GS3D_ERR_UNSUPPORTED);
  CHECK(gs3d_get_file_sections(&s)==GS3D_OK && s.med_lump_size==0UL);
  CHECK(gs3d_open("tests/fixtures/mdl7_with_med_lump.mdl")==GS3D_OK);
  CHECK(gs3d_get_file_sections(&s)==GS3D_OK && s.med_lump_size==11UL);
  CHECK(s.med_lump_start==s.mdl_data_end && s.trailing_bytes==0UL);
  CHECK(gs3d_open("tests/fixtures/mdl7_bad_med_overlap.mdl")==GS3D_ERR_RANGE);
  CHECK(gs3d_open("tests/fixtures/a4_mdl3.mdl")==GS3D_OK);
  CHECK(gs3d_get_material(0U,0U,&mi)==GS3D_OK && mi.has_image);
  CHECK(!mi.is_explicit && mi.opacity==65536L);
  puts("MATERIAL + MED LUMP TESTS PASS");return 0;
}
