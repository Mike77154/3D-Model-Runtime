/* Standalone regression entrypoint: C89 / native decoders only. */
#include "model_runtime89.h"
#include "modelrt_prj89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[192u*1024u*1024u];
static unsigned char prj_buffer[200000u];
static unsigned long tris;
static int on_tri(void*user,const modelrt_triangle_view*t)
{(void)user;(void)t;tris++;return 0;}
int main(int argc,char**argv)
{
    modelrt_runtime rt;modelrt_handle handle;modelrt_visit_sink sink;int rc,i,vr;
    char out[1024];modelrt_runtime_init(&rt);
    rc=modelrt_register_builtin_providers(&rt);
    if(rc || rt.provider_count!=75u){printf("REG_FAIL count=%lu rc=%d\n",(unsigned long)rt.provider_count,rc);return 2;}
    printf("REG_OK %lu\n",(unsigned long)rt.provider_count);
    rc=modelrt_prj89_summary("GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]",prj_buffer,sizeof(prj_buffer),out,sizeof(out));
    printf("PRJ rc=%d summary=%s\n",rc,rc==0?out:"-");
    memset(&sink,0,sizeof(sink));sink.on_triangle=on_tri;
    for(i=1;i<argc;i++){
        modelrt_handle_init(&handle,arena,sizeof(arena));
        rc=modelrt_open_file(&rt,&handle,argv[i]);
        if(rc){printf("REJECT %s rc=%d %s\n",argv[i],rc,modelrt_last_error(&rt));continue;}
        tris=0;vr=modelrt_visit(&handle,-1,0,&sink,0);
        printf("ACCEPT %s provider=%lu triangles=%lu visit=%d animation=%lu\n",argv[i],(unsigned long)handle.info.codec_id,tris,vr,(unsigned long)handle.info.animation_count);
        modelrt_close(&handle);
    }
    return 0;
}
