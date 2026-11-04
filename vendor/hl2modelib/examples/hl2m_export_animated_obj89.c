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
static hl2m_ik_pose bind_pose;
static hl2m_pose_matrices inverse_bind,anim_world;
static hl2m_skin_report skin_report;
static hl2m_native_anim_report89 native_report;
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

int main(int argc, char **argv)
{
    unsigned int ms,vs,ts;
    int r;
    int i,anim,frame;
    int entity_motion=0;
    int guard_mode=0,guard_limit=512;
    hl2m_vec3 motion_position;
    hl2m_fixed motion_yaw=0;
    if (argc<7 || argc>9) {
        fprintf(stderr,"usage: hl2m_export_animated_obj89 model.mdl model.vvd model.dx90.vtx animindex frame result.obj [--entity-motion | --guard-outliers [max_delta]]\n"); return 1;
    }
    if(argc>=8){
        if(strcmp(argv[7],"--entity-motion")==0 && argc==8)entity_motion=1;
        else if(strcmp(argv[7],"--guard-outliers")==0){
            guard_mode=1;
            if(argc==9)guard_limit=atoi(argv[8]);
            if(guard_limit<1 || guard_limit>16384)return 1;
        }else return 1;
    }
    if (load_bytes(argv[1],mdl_data,&ms,HL2M_FORMAT_MDL) ||
        load_bytes(argv[2],vvd_data,&vs,HL2M_FORMAT_VVD) ||
        load_bytes(argv[3],vtx_data,&ts,HL2M_FORMAT_VTX)) return 2;
    r=hl2m_master_decode89(HL2M_FORMAT_MDL,mdl_data,ms,0,0,&mdl); if (r!=HL2M_OK) {fprintf(stderr,"MDL error %d\n",r);return 3;}
    r=hl2m_master_decode89(HL2M_FORMAT_VVD,vvd_data,vs,0,0,&vvd); if (r!=HL2M_OK) {fprintf(stderr,"VVD error %d\n",r);return 4;}
    r=hl2m_master_decode89(HL2M_FORMAT_VTX,vtx_data,ts,0,0,&vtx); if (r!=HL2M_OK) {fprintf(stderr,"VTX error %d\n",r);return 5;}
    r=hl2m_check_mdl_vvd_vtx_sync(&mdl,&vvd,&vtx);
    if (r!=HL2M_OK) {fprintf(stderr,"checksum mismatch: %d\n",r);return 6;}
    r=hl2m_master_mesh89(&mdl,&vvd,&vtx,0,&engine);
    if (r!=HL2M_OK) {fprintf(stderr,"geometry error %d verts=%d idx=%d\n",r,engine.vertex_count,engine.index_count);return 7;}
    r=hl2m_engine_model_validate(&engine);
    if (r!=HL2M_OK) {fprintf(stderr,"draw validation error %d\n",r);return 8;}
    anim=atoi(argv[4]);frame=atoi(argv[5]);
    r=hl2m_native_bind_pose89(&mdl,&bind_pose);
    if(r!=HL2M_OK)return 10;
    r=hl2m_native_inverse_bind89(mdl_data,ms,&mdl,&inverse_bind);
    if(r!=HL2M_OK)return 11;
    r=frame==-1 ? hl2m_native_bind_matrices89(&mdl,&anim_world) :
        (guard_mode ? hl2m_master_anim_guard89(mdl_data,ms,0,0,&mdl,anim,frame,
            guard_limit*HL2M_FIXED_ONE,&anim_world,&native_report,&safety) :
            hl2m_master_anim89(mdl_data,ms,&mdl,anim,frame,&anim_world,&native_report));
    guarded_export=guard_mode;
    if(guard_mode)fprintf(stderr,"NON-NATIVE PREVIEW FALLBACK: %d axes / %d bones replaced; first bone=%d\n",
        safety.replaced_axes,safety.replaced_bones,safety.first_bone);
    if(r!=HL2M_OK){fprintf(stderr,"native anim error=%d anim=%d frame=%d\n",r,anim,frame);return 12;}
    r=hl2m_master_skin89(&engine,&inverse_bind,&anim_world,&animated,&skin_report);
    if(r!=HL2M_OK)return 13;
    engine=animated;
    if(entity_motion && frame>=0){
        r=hl2m_master_motion89(mdl_data,ms,&mdl,anim,frame,&motion_position,&motion_yaw);
        if(r!=HL2M_OK){fprintf(stderr,"root motion unsupported: %d\n",r);return 14;}
        /* Export only entity translation. Caller/presenter owns yaw rotation.
           Do not silently rotate vertices around an unverified pivot. */
        if(motion_yaw!=0) fprintf(stderr,"note: yaw reported but not applied by OBJ translation preview\n");
        for(i=0;i<engine.vertex_count;i++){
            engine.vertices[i].position.x+=motion_position.x;
            engine.vertices[i].position.y+=motion_position.y;
            engine.vertices[i].position.z+=motion_position.z;
        }
        fprintf(stderr,"SDK entity motion applied to OBJ: x=");
        { char buf[64];hl2m_fixed_to_string(motion_position.x,buf,sizeof(buf),3);fputs(buf,stderr); }
        fputc('\n',stderr);
    }
    if (dump_obj(argv[6])) return 9;
    printf("animation %s frame=%d frames=%d channels=%d quaternion=%d skinned=%d missingbones=%d\n",
        mdl.anims[anim].name,frame,native_report.frame_count,native_report.channels,
        native_report.raw_quaternion_channels,skin_report.skinned_count,skin_report.missing_bone_count);
    printf("OK model=%s mdlversion=%d sourceverts=%d triangles=%d draws=%d bones=%d warnings=%d\n",mdl.name,mdl.version,vvd.vertex_count,engine.index_count/3,engine.draw_count,mdl.bone_count,engine.warning_flags);
    for(i=0;i<engine.draw_count;++i) printf("material %d = %s triangles=%d\n",i,engine.draws[i].material_name,engine.draws[i].num_indices/3);
    return 0;
}
