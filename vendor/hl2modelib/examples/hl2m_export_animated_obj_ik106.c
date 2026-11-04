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
static hl2m_pose_matrices ik_temp;

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
    fprintf(f, "# generated with hl2modelib v1.0.6 - C89\nmtllib %s\no t103_native\n", base);
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

/* Per-frame stance approach: contact anchors live in entity/world coordinates.
   Motion extracted from native MDL, bone matrices remain in model space.
   Raw animation/sequences/IK rule curves are NOT yet evaluated by this preview. */
static int find_bone(const char *name)
{
    int i;
    for(i=0;i<mdl.bone_count;i++)if(!strcmp(mdl.bones[i].name,name))return i;
    return -1;
}
static int grounded_anim(int anim,int last,unsigned int ms)
{
    int hip[2],knee[2],foot[2],i,f,k,r,lock,plants=0,corrected_count=0;
    hl2m_vec3 movement,world,target;hl2m_fixed yaw,ground=2147483647;
    hl2m_foot_lock89 stance[2];hl2m_ik_chain89 chain;hl2m_ik_chain_report89 rep;
    hip[0]=find_bone("L_momo");hip[1]=find_bone("R_momo");
    knee[0]=find_bone("L_hiza");knee[1]=find_bone("R_hiza");
    foot[0]=find_bone("L_ashi");foot[1]=find_bone("R_ashi");
    if(hip[0]<0 || hip[1]<0){
        hip[0]=find_bone("_22");hip[1]=find_bone("_26");
        knee[0]=find_bone("_23");knee[1]=find_bone("_27");
        foot[0]=find_bone("_24");foot[1]=find_bone("_28");
    }
    if(hip[0]<0||knee[0]<0||foot[0]<0||hip[1]<0||knee[1]<0||foot[1]<0)return HL2M_ERR_UNSUPPORTED;
    memset(stance,0,sizeof(stance));
    /* Fixed reference ground: minimum of uncorrected feet across entire native cycle. */
    for(f=0;f<mdl.anims[anim].numframes;f++){
        r=hl2m_master_anim89(mdl_data,ms,&mdl,anim,f,&anim_world,&native_report);if(r)return r;
        for(k=0;k<2;k++)if(anim_world.world[foot[k]].m[2][3]<ground)
            ground=anim_world.world[foot[k]].m[2][3];
    }
    for(f=0;f<=last;f++){
        r=hl2m_master_anim89(mdl_data,ms,&mdl,anim,f,&anim_world,&native_report);if(r)return r;
        r=hl2m_master_motion89(mdl_data,ms,&mdl,anim,f,&movement,&yaw);if(r)return r;
        for(k=0;k<2;k++){
            world.x=anim_world.world[foot[k]].m[0][3]+movement.x;
            world.y=anim_world.world[foot[k]].m[1][3]+movement.y;
            world.z=anim_world.world[foot[k]].m[2][3]+movement.z;
            r=hl2m_master_foot_lock89(&stance[k],world,ground,3*HL2M_FIXED_ONE,
                4*HL2M_FIXED_ONE,&target,&lock);if(r)return r;
            if(!lock)continue;
            plants++;
            memset(&chain,0,sizeof(chain));chain.hip=hip[k];chain.knee=knee[k];chain.foot=foot[k];
            chain.goal.x=target.x-movement.x;chain.goal.y=target.y-movement.y;
            chain.goal.z=target.z-movement.z;chain.weight=HL2M_FIXED_ONE;
            r=hl2m_master_ik_two_bone89(&anim_world,&chain,&ik_temp,&rep);
            if(r)return r;
            if(rep.applied)corrected_count++;
            anim_world=ik_temp;
        }
    }
    i=last;
    fprintf(stderr,"IK ROOT STANCE frame=%d samples=%d planted_samples=%d corrections=%d ground=",i,last+1,plants,corrected_count);
    {char buf[40];hl2m_fixed_to_string(ground,buf,sizeof(buf),2);fputs(buf,stderr);}
    fputc('\n',stderr);
    return HL2M_OK;
}

int main(int argc, char **argv)
{
    unsigned int ms,vs,ts;
    int r;
    int i,anim,frame;
    int entity_motion=0,ik_grounded=0;
    hl2m_vec3 motion_position;
    hl2m_fixed motion_yaw=0;
    if (argc != 7 && argc != 8) {
        fprintf(stderr,"usage: hl2m_export_animated_obj_ik106 model.mdl model.vvd model.dx90.vtx animindex frame result.obj [--entity-motion|--ik-grounded]\n"); return 1;
    }
    if(argc==8){
        if(strcmp(argv[7],"--entity-motion")==0)entity_motion=1;
        else if(strcmp(argv[7],"--ik-grounded")==0)ik_grounded=1;
        else return 1;
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
    r=frame==-1 ? hl2m_native_bind_matrices89(&mdl,&anim_world) : hl2m_master_anim89(mdl_data,ms,&mdl,anim,frame,&anim_world,&native_report);
    if(r!=HL2M_OK){fprintf(stderr,"native anim error=%d anim=%d frame=%d\n",r,anim,frame);return 12;}
    if(ik_grounded && frame>=0){r=grounded_anim(anim,frame,ms);if(r){fprintf(stderr,"IK error %d\n",r);return 15;}}
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
