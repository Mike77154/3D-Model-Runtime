/* diagnostic-only SMD export using hl2modelib; strict C89 */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
static unsigned char smd_bytes[16u*1024u*1024u];
static hl2m_smd smd;
static hl2m_engine_model engine;
static void val(FILE *f, hl2m_fixed v)
{
    char tmp[48];
    hl2m_fixed_to_string(v, tmp, sizeof(tmp), 5);
    fputs(tmp, f);
}

static int dump_obj(const char *objpath)
{
    FILE *f;
    FILE *mtl;
    int i;
    int d;
    int k;
    int a, b, c;
    char mtlpath[1024];
    const char *base;
    unsigned int n;
    n = (unsigned int)strlen(objpath);
    if (n >= sizeof(mtlpath)-5u) return 1;
    strcpy(mtlpath, objpath);
    if (n>=4u && strcmp(mtlpath+n-4u, ".obj")==0) strcpy(mtlpath+n-4u, ".mtl");
    else strcat(mtlpath, ".mtl");
    base = strrchr(mtlpath, '/');
    if (!base) base = strrchr(mtlpath, '\\');
    base = base ? base+1 : mtlpath;
    f = fopen(objpath,"wb");
    if (!f) return 1;
    mtl = fopen(mtlpath,"wb");
    if (!mtl) { fclose(f); return 1; }
    fprintf(f, "# generated with hl2modelib v1.0.1 - C89\nmtllib %s\no t103_native\n", base);
    for (i=0; i<engine.vertex_count; ++i) {
        fputs("v ",f); val(f,engine.vertices[i].position.x);
        fputc(' ',f); val(f,engine.vertices[i].position.y);
        fputc(' ',f); val(f,engine.vertices[i].position.z); fputc('\n',f);
    }
    for (i=0; i<engine.vertex_count; ++i) {
        fputs("vt ",f);val(f,engine.vertices[i].texcoord.x);fputc(' ',f);
        val(f,HL2M_FIXED_ONE-engine.vertices[i].texcoord.y);fputc('\n',f);
    }
    for (i=0; i<engine.vertex_count; ++i) {
        fputs("vn ",f);val(f,engine.vertices[i].normal.x);fputc(' ',f);
        val(f,engine.vertices[i].normal.y);fputc(' ',f);
        val(f,engine.vertices[i].normal.z);fputc('\n',f);
    }
    for (d=0; d<engine.draw_count; ++d) {
        const hl2m_engine_draw *dr=&engine.draws[d];
        const char *name=dr->replacement_material[0] ? dr->replacement_material : dr->material_name;
        if (!name[0]) name="default";
        fprintf(f,"g draw_%d\nusemtl %s\n",d,name);
        fprintf(mtl,"newmtl %s\nKa 0.20 0.20 0.20\nKd 0.85 0.85 0.85\nKs 0.03 0.03 0.03\nmap_Kd %s.png\n\n",name,name);
        for (k=dr->first_index; k+2<dr->first_index+dr->num_indices; k+=3) {
            a=engine.indices[k]+1;b=engine.indices[k+1]+1;c=engine.indices[k+2]+1;
            fprintf(f,"f %d/%d/%d %d/%d/%d %d/%d/%d\n",a,a,a,b,b,b,c,c,c);
        }
    }
    fclose(mtl); fclose(f);return 0;
}

int main(int argc,char**argv) {
 FILE *f; unsigned int sz; int r;
 if(argc!=3){puts("usage: smd_to_obj89 input.smd output.obj");return 1;}
 f=fopen(argv[1],"rb");if(!f)return 2;
 sz=(unsigned int)fread(smd_bytes,1,sizeof(smd_bytes),f);fclose(f);
 r=hl2m_decode_smd(smd_bytes,sz,&smd);if(r){printf("smd decode=%d\n",r);return 3;}
 r=hl2m_build_engine_model_from_smd(&smd,&engine);if(r){printf("mesher=%d\n",r);return 4;}
 {
 int i;
 for(i=0;i<engine.draw_count;i++) {
  int m=engine.draws[i].material;
  if(m>=0 && m<smd.material_count){
   strncpy(engine.draws[i].material_name,smd.materials[m],HL2M_PATH_MAX-1);
   engine.draws[i].material_name[HL2M_PATH_MAX-1]=0;
  }
 }
 }
 r=hl2m_engine_model_validate(&engine);if(r){printf("validate=%d\n",r);return 5;}
 if(dump_obj(argv[2]))return 6;
 printf("OK verts=%d tris=%d draws=%d nodes=%d\n",engine.vertex_count,engine.index_count/3,engine.draw_count,smd.node_count);
 return 0;
}
