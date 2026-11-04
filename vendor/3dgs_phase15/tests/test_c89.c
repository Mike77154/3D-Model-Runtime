#include "../include/gs3d.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(b) do { if(!(b)) {printf("FAIL line %d\n",__LINE__); return 1;} }while(0)
static int try_valid(const char *path,int fmt,unsigned int groups){
    int r; unsigned int i;unsigned long w=0UL,h=0UL;
    r=gs3d_open(path); CHECK(r==0);CHECK(gs3d_model.format==fmt);
    CHECK(gs3d_model.groups==groups);
    CHECK(gs3d_model.vertices==3UL*(unsigned long)groups);
    CHECK(gs3d_model.faces==(unsigned long)groups);
    CHECK(gs3d_model.group[0].frame_count>0);
    CHECK(gs3d_model.face[0].vi[1]==1U);
    for(i=0;i<groups;i++){
        CHECK(gs3d_apply_frame(i,0)==0);
        CHECK(gs3d_model.v[gs3d_model.group[i].vertex_first+1UL].x == 65536L || fmt==7);
        CHECK(gs3d_decode_skin(i,0,&w,&h)==0);
        CHECK(w==2UL && h==2UL);
        CHECK(gs3d_rgba[3UL]==255U);
    }
    gs3d_close();return 0;
}

/* Test custom file-package source: no malloc, no filesystem needed by parser. */
typedef struct BufferIO {const unsigned char *data;unsigned long size;} BufferIO;
static int memory_reader(void *userdata,unsigned long offset,unsigned char *out,unsigned long n){
    BufferIO *b=(BufferIO *)userdata;
    if(offset>b->size||n>b->size-offset)return 1;
    memcpy(out,b->data+offset,(size_t)n);return 0;
}
static int test_memory_and_callback(void){
    static unsigned char data[2048];
    FILE *f=fopen("tests/fixtures/mdl7_bone_keys.mdl","rb");
    size_t n;GS3D_BoneKey key;GS3D_Source io;BufferIO buffer;
    CHECK(f!=NULL);n=fread(data,1,sizeof(data),f);fclose(f);
    CHECK(n>500U && n<sizeof(data));
    CHECK(gs3d_open_memory(data,(unsigned long)n)==GS3D_OK);
    CHECK(gs3d_model.groups==2U);
    CHECK(gs3d_model.group[0].frames[0].trans_count==1UL);
    CHECK(gs3d_read_bone_key(0U,0U,0UL,&key)==GS3D_OK);
    CHECK(key.bone==0U);CHECK(key.matrix[12]==196608L);
    CHECK(key.matrix[13]==262144L);CHECK(key.matrix[14]==327680L);
    gs3d_close();
    buffer.data=data;buffer.size=(unsigned long)n;
    io.read_at=memory_reader;io.userdata=&buffer;io.size=buffer.size;
    CHECK(gs3d_open_source(&io)==GS3D_OK);
    CHECK(gs3d_apply_frame(0U,0U)==GS3D_OK);
    CHECK(gs3d_read_bone_key(1U,0U,0UL,&key)==GS3D_OK);
    gs3d_close();
    return 0;
}
int main(void){
    int r;int i;char path[300];
    for(i=0;i<256;i++){
       gs3d_palette[i*3]=gs3d_palette[i*3+1]=gs3d_palette[i*3+2]=(unsigned char)i;
    }
    strcpy(path,"tests/fixtures/a4_mdl3.mdl");CHECK(try_valid(path,3,1)==0);
    strcpy(path,"tests/fixtures/a5_mdl4.mdl");CHECK(try_valid(path,4,1)==0);
    strcpy(path,"tests/fixtures/a6_mdl5.mdl");CHECK(try_valid(path,5,1)==0);
    strcpy(path,"tests/fixtures/a7_a8_mdl7.mdl");CHECK(try_valid(path,7,2)==0);
    strcpy(path,"tests/fixtures/mdl7_bones.mdl");CHECK(try_valid(path,7,2)==0);
    strcpy(path,"tests/fixtures/bad_mdl7_index.mdl");r=gs3d_open(path);CHECK(r==GS3D_ERR_CORRUPT);
    strcpy(path,"tests/fixtures/bad_mdl4_truncated.mdl");r=gs3d_open(path);CHECK(r==GS3D_ERR_RANGE);
    strcpy(path,"tests/fixtures/bad_signature.mdl");r=gs3d_open(path);CHECK(r==GS3D_ERR_MAGIC);
    CHECK(gs3d_a4_open("tests/fixtures/a4_mdl3.mdl")==GS3D_OK);gs3d_close();
    CHECK(gs3d_a5_open("tests/fixtures/a5_mdl4.mdl")==GS3D_OK);gs3d_close();
    CHECK(gs3d_a6_open("tests/fixtures/a6_mdl5.mdl")==GS3D_OK);gs3d_close();
    CHECK(gs3d_a7_open("tests/fixtures/a7_a8_mdl7.mdl")==GS3D_OK);gs3d_close();
    CHECK(gs3d_a8_open("tests/fixtures/a7_a8_mdl7.mdl")==GS3D_OK);gs3d_close();
    CHECK(gs3d_a4_open("tests/fixtures/a7_a8_mdl7.mdl")==GS3D_ERR_UNSUPPORTED);
    CHECK(test_memory_and_callback()==0);
    r=gs3d_open("tests/fixtures/bad_mdl7_bone_cycle.mdl");CHECK(r==GS3D_ERR_CORRUPT);
    CHECK(gs3d_detect_magic((const unsigned char *)"MDL7",4UL)==7);
    CHECK(gs3d_family_supports(4U,7)==0);
    CHECK(gs3d_family_supports(8U,7)!=0);
    puts("ALL TESTS PASSED: 6 valid + memory/callback, 4 rejected, 5 vendor profiles, registry");return 0;
}
