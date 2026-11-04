/* hl2modelib offline OBJ probe: real MDL/VVD/VTX triangles and UVs.
   Strict ANSI C89, fixed-point model coordinates, no heap. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>

#define HL2M_PROBE_CAP (8u * 1024u * 1024u)
static hl2m_u8 mdl_data[HL2M_PROBE_CAP];
static hl2m_u8 vvd_data[HL2M_PROBE_CAP];
static hl2m_u8 vtx_data[HL2M_PROBE_CAP];
static hl2m_mdl mdl;
static hl2m_vvd vvd;
static hl2m_vtx vtx;
static hl2m_engine_model engine;

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

int main(int argc, char **argv)
{
    unsigned int ms,vs,ts;
    int r;
    int i;
    if (argc != 5) {
        fprintf(stderr,"usage: hl2m_export_obj89 model.mdl model.vvd model.dx90.vtx result.obj\n"); return 1;
    }
    if (load_bytes(argv[1],mdl_data,&ms,HL2M_FORMAT_MDL) ||
        load_bytes(argv[2],vvd_data,&vs,HL2M_FORMAT_VVD) ||
        load_bytes(argv[3],vtx_data,&ts,HL2M_FORMAT_VTX)) return 2;
    r=hl2m_decode_mdl(mdl_data,ms,&mdl); if (r!=HL2M_OK) {fprintf(stderr,"MDL error %d\n",r);return 3;}
    r=hl2m_decode_vvd(vvd_data,vs,0,&vvd); if (r!=HL2M_OK) {fprintf(stderr,"VVD error %d\n",r);return 4;}
    r=hl2m_decode_vtx(vtx_data,ts,&vtx); if (r!=HL2M_OK) {fprintf(stderr,"VTX error %d\n",r);return 5;}
    r=hl2m_check_mdl_vvd_vtx_sync(&mdl,&vvd,&vtx);
    if (r!=HL2M_OK) {fprintf(stderr,"checksum mismatch: %d\n",r);return 6;}
    r=hl2m_build_engine_model(&mdl,&vvd,&vtx,0,&engine);
    if (r!=HL2M_OK) {fprintf(stderr,"geometry error %d verts=%d idx=%d\n",r,engine.vertex_count,engine.index_count);return 7;}
    r=hl2m_engine_model_validate(&engine);
    if (r!=HL2M_OK) {fprintf(stderr,"draw validation error %d\n",r);return 8;}
    if (dump_obj(argv[4])) return 9;
    printf("OK model=%s mdlversion=%d sourceverts=%d triangles=%d draws=%d bones=%d warnings=%d\n",mdl.name,mdl.version,vvd.vertex_count,engine.index_count/3,engine.draw_count,mdl.bone_count,engine.warning_flags);
    for(i=0;i<engine.draw_count;++i) printf("material %d = %s triangles=%d\n",i,engine.draws[i].material_name,engine.draws[i].num_indices/3);
    return 0;
}
