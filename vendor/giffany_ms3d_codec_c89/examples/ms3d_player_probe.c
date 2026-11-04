/* Inspect animation frames without graphics dependency. CSV output. */
#include "gif_ms3d_player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BUFFER_CAP (16UL*1024UL*1024UL)
static unsigned char bytes[BUFFER_CAP];
static ms3d_model model;
static ms3d_skin_pose pose;
static ms3d_skin_vertices vertices;
static ms3d_corner_normals normals;
static ms3d_uv_bindings bindings;
static ms3d_player player;
typedef struct stats_s {
    unsigned long frames,triangles,corners;
    ms3d_fx p[3],uv[2];int material;
} stats;
static stats count;
static int callback_begin(void *user,unsigned long ms,ms3d_fx time,const ms3d_model *m)
{(void)ms;(void)time;(void)m;((stats *)user)->triangles=0UL;((stats *)user)->corners=0UL;return 0;}
static int callback_tri(void *user,ms3d_u16 i,int g,int m)
{(void)i;(void)g;(void)m;++((stats *)user)->triangles;return 0;}
static int callback_corner(void *user,ms3d_u16 i,ms3d_u8 c,const ms3d_draw_corner *d)
{
    stats *s=(stats *)user;unsigned k;
    ++s->corners;
    if(i==0U&&c==0U){for(k=0U;k<3U;++k)s->p[k]=d->position[k];
        s->uv[0]=d->uv[0];s->uv[1]=d->uv[1];s->material=d->material_index;}
    return 0;
}
static void printfx(ms3d_fx n)
{char buf[64];if(ms3d_fx_to_ascii(n,buf,sizeof(buf),5U)==MS3D_OK)printf("%s",buf);else printf("0");}
int main(int argc,char **argv)
{
    FILE *f;size_t got=0U;unsigned long dt,steps,k;int rc,extra;
    ms3d_player_sink sink;
    if(argc<2||argc>4){fprintf(stderr,"usage: ms3d_player_probe model.ms3d [step_ms] [frames]\n");return 2;}
    dt=argc>=3?(unsigned long)atol(argv[2]):250UL;
    steps=argc>=4?(unsigned long)atol(argv[3]):9UL;
    if(dt==0UL||dt>30000UL||steps<1UL||steps>1000UL)return 2;
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    got=fread(bytes,1U,sizeof(bytes),f);extra=fgetc(f);fclose(f);
    if(got==0U||extra!=EOF){fputs("input too large/empty\n",stderr);return 2;}
    rc=ms3d_parse(&model,bytes,got);
    if(rc!=MS3D_OK){fprintf(stderr,"parse error %d\n",rc);return 1;}
    rc=ms3d_player_prepare(&player,&model,&pose,&vertices,&normals,&bindings,MS3D_NORMAL_SOURCE);
    if(rc!=MS3D_OK){fprintf(stderr,"prepare error %d\n",rc);return 1;}
    printf("# joints=%u materials=%u triangles=%u duration_ms=%lu\n",
        (unsigned)model.num_joints,(unsigned)model.num_materials,
        (unsigned)model.num_triangles,player.duration_ms);
    for(k=0U;k<model.num_materials;++k)
        printf("# material=%lu name=%s diffuse=%s alphamap=%s\n",k,
            model.materials[k].name,model.materials[k].texture,
            model.materials[k].alphamap);
    memset(&sink,0,sizeof(sink));sink.user=&count;sink.begin_frame=callback_begin;
    sink.begin_triangle=callback_tri;sink.corner=callback_corner;
    puts("frame,ms,triangles,corners,material_0,x0,y0,z0,u0,v0");
    for(k=0UL;k<steps;++k){
        if(ms3d_player_emit(&player,&sink)!=MS3D_OK)return 1;
        printf("%lu,%lu,%lu,%lu,%d,",k,player.current_ms,count.triangles,
            count.corners,count.material);
        printfx(count.p[0]);putchar(',');printfx(count.p[1]);putchar(',');
        printfx(count.p[2]);putchar(',');printfx(count.uv[0]);putchar(',');
        printfx(count.uv[1]);putchar('\n');
        if(ms3d_player_advance(&player,dt)!=MS3D_OK)return 1;
    }
    return 0;
}
