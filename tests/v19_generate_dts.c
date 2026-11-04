#include "dts89.h"
#include <stdio.h>
#include <string.h>
static unsigned char g_buffer[16384];
int main(int argc,char **argv)
{
    dts89_shape s;dts89_mesh_view m;dts89_material mat;dts89_writer w;FILE *f;int rc;
    if(argc<2)return 2;
    memset(&s,0,sizeof(s));memset(&m,0,sizeof(m));memset(&mat,0,sizeof(mat));
    s.header.read_version=DTS89_VERSION_CURRENT;s.header.exporter_version=DTS89_EXPORTER_CURRENT;
    s.num_meshes=1;s.meshes=&m;m.mesh_type=DTS89_MESH_NULL;
    s.num_materials=1;s.materials=&mat;mat.name.ptr="fixture";mat.name.len=7;
    mat.bump_map=mat.detail_map=0xffffffffu;
    mat.detail_scale_bits=DTS89_F32_BITS_ONE;
    dts89_writer_init(&w,g_buffer,sizeof(g_buffer));
    rc=dts89_encode_shape(&s,&w,0);
    if(rc){printf("dts encode rc=%d\n",rc);return 3;}
    f=fopen(argv[1],"wb");if(!f)return 4;
    if(fwrite(g_buffer,1,w.pos,f)!=w.pos){fclose(f);return 5;}
    fclose(f);printf("dts generated %u bytes\n",(unsigned)w.pos);
    return 0;
}
