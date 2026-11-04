/* hl2modelib offline OBJ probe: real MDL/VVD/VTX triangles and UVs.
   Strict ANSI C89, fixed-point model coordinates, no heap. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define HL2M_PROBE_CAP (8u * 1024u * 1024u)
static hl2m_u8 mdl_data[HL2M_PROBE_CAP];
static hl2m_u8 vvd_data[HL2M_PROBE_CAP];
static hl2m_u8 vtx_data[HL2M_PROBE_CAP];
static hl2m_mdl mdl;
static hl2m_vvd vvd;
static hl2m_vtx vtx;
static hl2m_engine_model engine;
static hl2m_engine_model animated;
static hl2m_pose_matrices inverse_bind,anim_world;
static hl2m_skin_report skin_report;
static hl2m_anim_safety_report89 safety;
static int guarded_export=0;

static int load_bytes(const char *path, hl2m_u8 *dst, unsigned int *size, int format)
{
    FILE *file;
    size_t count;
    int fixed;
    int r;
    file = fopen(path, "rb");
    if (!file) { fprintf(stderr, "missing: %s\n", path); return 1; }
    count = fread(dst, 1u, HL2M_PROBE_CAP, file);
    if (!feof(file)) { fprintf(stderr, "file too large: %s\n", path); fclose(file); return 1; }
    fclose(file);
    *size = (unsigned int)count;
    r = hl2m_repair_missing_first_byte(dst, size, HL2M_PROBE_CAP, format, &fixed);
    if (r != HL2M_OK) { fprintf(stderr, "invalid source signature %s: %d\n", path, r); return 1; }
    if (fixed) fprintf(stderr, "repaired 1-byte header: %s\n", path);
    return 0;
}

static void val(FILE *f, hl2m_fixed v)
{
    char tmp[48];
    hl2m_fixed_to_string(v, tmp, sizeof(tmp), 5);
    fputs(tmp, f);
}

/* Source paths can use Windows backslashes; OBJ object tokens cannot. */
static void obj_model_token(char *out, unsigned int cap, const char *source_name)
{
    const char *part=source_name;
    const char *p;
    unsigned int n=0u;
    unsigned char c;
    if(!out || cap==0u)return;
    for(p=source_name;*p;++p){
        if(*p=='/' || *p=='\\')part=p+1;
    }
    for(p=part;*p && *p!='.' && n+1u<cap;++p){
        c=(unsigned char)*p;
        if((c>='a' && c<='z') || (c>='A' && c<='Z') ||
           (c>='0' && c<='9') || c=='_' || c=='-')out[n++]=(char)c;
        else out[n++]='_';
    }
    if(n==0u){
        const char *fallback="source_model";
        while(*fallback && n+1u<cap)out[n++]=*fallback++;
    }
    out[n]='\0';
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
    char objname[HL2M_NAME_MAX];
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
    obj_model_token(objname,sizeof(objname),mdl.name);
    fprintf(f, "# generated with hl2modelib %d.%d.%d - C89\nmtllib %s\no %s\n",
            HL2M_VERSION_MAJOR,HL2M_VERSION_MINOR,HL2M_VERSION_PATCH,base,objname);
    if(guarded_export) fprintf(f,"# NON-NATIVE PREVIEW: %d guarded translation axes, %d bones\n",safety.replaced_axes,safety.replaced_bones);
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

int main(int argc,char **argv)
{
    unsigned int ms,vs,ts;int seq,cycle,px=0,py=0,ids[4],r,layered=0;
    hl2m_sequence_layers_report89 layers_report;
    if(argc<7 || argc>10){
        fputs("usage: sequence_obj119 mdl vvd vtx seqindex cycle65536 out.obj [param_x65536] [param_y65536] [use_layers0or1]\n",stderr);
        return 1;
    }
    if(load_bytes(argv[1],mdl_data,&ms,HL2M_FORMAT_MDL) ||
       load_bytes(argv[2],vvd_data,&vs,HL2M_FORMAT_VVD) ||
       load_bytes(argv[3],vtx_data,&ts,HL2M_FORMAT_VTX))return 2;
    r=hl2m_master_decode89(HL2M_FORMAT_MDL,mdl_data,ms,0,0,&mdl);if(r)return 3;
    r=hl2m_master_decode89(HL2M_FORMAT_VVD,vvd_data,vs,0,0,&vvd);if(r)return 4;
    r=hl2m_master_decode89(HL2M_FORMAT_VTX,vtx_data,ts,0,0,&vtx);if(r)return 5;
    r=hl2m_check_mdl_vvd_vtx_sync(&mdl,&vvd,&vtx);if(r)return 6;
    r=hl2m_master_mesh89(&mdl,&vvd,&vtx,0,&engine);if(r)return 7;
    r=hl2m_native_inverse_bind89(mdl_data,ms,&mdl,&inverse_bind);if(r)return 8;
    seq=atoi(argv[4]);cycle=atoi(argv[5]);
    if(argc>=8)px=atoi(argv[7]);
    memset(ids,0,sizeof(ids));
    if(argc>=9)py=atoi(argv[8]);
    if(argc>=10)layered=atoi(argv[9]);
    if(layered){
        r=hl2m_master_sequence_layered89(mdl_data,ms,NULL,0,&mdl,seq,cycle,px,py,&anim_world,&layers_report);
    }else r=hl2m_master_sequence89(mdl_data,ms,NULL,0,&mdl,seq,cycle,px,py,&anim_world,ids);
    if(r){fprintf(stderr,"sequence error %d\n",r);return 9;}
    if(layered) printf("layers applied=%d declared=%d delta=%d\n",layers_report.applied,layers_report.declared,layers_report.child_delta);
    r=hl2m_master_skin89(&engine,&inverse_bind,&anim_world,&animated,&skin_report);
    if(r)return 10;
    engine=animated;
    if(dump_obj(argv[6]))return 11;
    printf("sequence=%s cycle=%d params=%d,%d cells=%d,%d,%d,%d verts=%d triangles=%d missing=%d\n",
      mdl.sequences[seq].label,cycle,px,py,ids[0],ids[1],ids[2],ids[3],engine.vertex_count,
      engine.index_count/3,skin_report.missing_bone_count);
    return 0;
}
