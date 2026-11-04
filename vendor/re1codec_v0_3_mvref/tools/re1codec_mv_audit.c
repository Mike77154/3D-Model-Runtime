#include "re1codec.h"
#include <stdio.h>
#include <string.h>

#define MV_MAX_FILE (2u * 1024u * 1024u)
static re1c_u8 g_file[MV_MAX_FILE];

typedef struct mv_counts_s {
    re1c_u32 checks;
    re1c_u32 raw_bytes;
    re1c_u32 banks;
    re1c_u32 parts;
    re1c_u32 children;
    re1c_u32 clips;
    re1c_u32 frame_refs;
    re1c_u32 poses;
    re1c_u32 angles;
    re1c_u32 objects;
    re1c_u32 vertices;
    re1c_u32 normals;
    re1c_u32 primitives;
    re1c_u32 mv_stride_quirks;
} mv_counts;

static re1c_u16 rd16(const re1c_u8 *p)
{
    return (re1c_u16)((re1c_u16)p[0] | (re1c_u16)((re1c_u16)p[1] << 8));
}

static re1c_u32 rd32(const re1c_u8 *p)
{
    return (re1c_u32)p[0] | ((re1c_u32)p[1] << 8) |
           ((re1c_u32)p[2] << 16) | ((re1c_u32)p[3] << 24);
}

static int range_ok(re1c_u32 pos, re1c_u32 len, re1c_u32 size)
{
    if (pos > size) return 0;
    if (len > size - pos) return 0;
    return 1;
}

static int fail(const char *what, re1c_u32 a, re1c_u32 b)
{
    fprintf(stderr, "MISMATCH %s ref=%lu codec=%lu\n", what,
            (unsigned long)a, (unsigned long)b);
    return 0;
}

static int check_u32(mv_counts *c, const char *what, re1c_u32 a, re1c_u32 b)
{
    c->checks++;
    if (a != b) return fail(what, a, b);
    return 1;
}

static int check_i32(mv_counts *c, const char *what, re1c_i32 a, re1c_i32 b)
{
    c->checks++;
    if (a != b) {
        fprintf(stderr, "MISMATCH %s ref=%ld codec=%ld\n", what,
                (long)a, (long)b);
        return 0;
    }
    return 1;
}

static re1c_i32 mv_pos_centi(re1c_u16 w)
{
    if (w > 32767u) return (re1c_i32)(65535u - (re1c_u32)w);
    return -(re1c_i32)w;
}

static int mv_alpha(const re1c_u8 *d, re1c_u32 size)
{
    re1c_u32 tim_off;
    re1c_u32 palette_size;
    re1c_u32 image_pos;
    re1c_u32 image_size;
    re1c_u32 tim_size;
    if (size < 20u) return 0;
    tim_off = rd32(d + size - 4u);
    if (!range_ok(tim_off, 12u, size)) return 0;
    palette_size = rd32(d + tim_off + 8u);
    if (palette_size < 4u) return 0;
    image_pos = tim_off + 8u + (palette_size - 4u);
    if (!range_ok(image_pos, 4u, size)) return 0;
    image_size = rd32(d + image_pos);
    tim_size = palette_size + image_size + 8u;
    if (tim_size > size - tim_off) return 0;
    return (size - (tim_size + tim_off) == 12u) ? 1 : 0;
}

static int compare_section(mv_counts *c, const char *name,
                           const re1c_section *sec,
                           const re1c_u8 *d, re1c_u32 off, re1c_u32 len)
{
    c->checks += 2u;
    if (sec->source_offset != off) return fail(name, off, sec->source_offset);
    if (sec->size != len) return fail(name, len, sec->size);
    if (len != 0u) {
        c->checks++;
        c->raw_bytes += len;
        if (memcmp(sec->data, d + off, (size_t)len) != 0) {
            fprintf(stderr, "MISMATCH %s raw bytes\n", name);
            return 0;
        }
    }
    return 1;
}

static int ref_parent(const re1c_u8 *emr, re1c_u32 emr_size,
                      re1c_u16 part_off, re1c_u16 part_count,
                      re1c_u32 object_index, re1c_i16 *out_parent)
{
    re1c_u32 i;
    *out_parent = (re1c_i16)-1;
    for (i = 0u; i < (re1c_u32)part_count; ++i) {
        re1c_u16 count;
        re1c_u16 rel;
        re1c_u32 j;
        if (!range_ok((re1c_u32)part_off + i * 4u, 4u, emr_size)) return 0;
        count = rd16(emr + part_off + i * 4u);
        rel = rd16(emr + part_off + i * 4u + 2u);
        if (!range_ok((re1c_u32)part_off + (re1c_u32)rel, (re1c_u32)count, emr_size)) return 0;
        for (j = 0u; j < (re1c_u32)count; ++j) {
            if ((re1c_u32)emr[part_off + rel + j] == object_index) {
                *out_parent = (re1c_i16)i;
                return 1;
            }
        }
    }
    return 1;
}

static int ref_global(const re1c_u8 *emr, re1c_u32 emr_size,
                      re1c_u16 part_off, re1c_u16 part_count,
                      re1c_u32 object_index, re1c_vec3_centi *out)
{
    re1c_i16 cur;
    re1c_u32 guard;
    out->x = 0; out->y = 0; out->z = 0;
    cur = (re1c_i16)object_index;
    guard = 0u;
    while (cur >= 0) {
        const re1c_u8 *p;
        re1c_i16 parent;
        if (guard++ > (re1c_u32)part_count) return 0;
        if (!range_ok(8u + (re1c_u32)cur * 6u, 6u, emr_size)) return 0;
        p = emr + 8u + (re1c_u32)cur * 6u;
        out->z += mv_pos_centi(rd16(p + 0u));
        out->y += mv_pos_centi(rd16(p + 2u));
        out->x += mv_pos_centi(rd16(p + 4u));
        if (!ref_parent(emr, emr_size, part_off, part_count, (re1c_u32)cur, &parent)) return 0;
        cur = parent;
    }
    return 1;
}

static int audit_bank(mv_counts *c, const re1c_view *view, re1c_u32 bank,
                      const re1c_u8 *emr, re1c_u32 emr_size,
                      const re1c_u8 *edd, re1c_u32 edd_size)
{
    re1c_bank_info bi;
    re1c_u16 part_off, anim_off, part_count, frame_size;
    re1c_u32 pose_count, clip_count, type_value;
    re1c_u32 i, j;
    int rc;
    if (emr_size == 0u && edd_size == 0u) return 1;
    if (emr_size < 8u || edd_size < 4u) return 0;
    part_off = rd16(emr + 0u);
    anim_off = rd16(emr + 2u);
    part_count = rd16(emr + 4u);
    frame_size = rd16(emr + 6u);
    if (frame_size == 0u || anim_off >= emr_size) return 0;
    pose_count = (emr_size - 4u - (re1c_u32)anim_off) / (re1c_u32)frame_size;
    clip_count = (re1c_u32)rd16(edd + 2u) / 4u;
    type_value = 0u;
    for (i = 0u; i < (re1c_u32)part_count; ++i)
        type_value ^= rd32(emr + part_off + i * 4u);

    rc = re1c_get_bank_info(view, bank, &bi);
    if (rc != RE1C_OK) { fprintf(stderr, "bank info rc=%d\n", rc); return 0; }
    c->banks++;
    if (!check_u32(c,"bank.part_off",part_off,bi.part_list_offset)) return 0;
    if (!check_u32(c,"bank.anim_off",anim_off,bi.animation_offset)) return 0;
    if (!check_u32(c,"bank.object_count",part_count,bi.object_count)) return 0;
    if (!check_u32(c,"bank.frame_size",frame_size,bi.frame_size)) return 0;
    if (!check_u32(c,"bank.pose_count",pose_count,bi.pose_count)) return 0;
    if (!check_u32(c,"bank.clip_count",clip_count,bi.clip_count)) return 0;
    if (!check_u32(c,"bank.emd_type",type_value,bi.emd_type)) return 0;
    {
        int skip_byte;
        re1c_u32 mv_stride;
        skip_byte = 1;
        if (type_value == 0x000B000Bu || type_value == 0x000A0001u ||
            type_value == 0x00190009u || type_value == 0x00000001u ||
            type_value == 0x00080005u) skip_byte = 0;
        mv_stride = 12u + (re1c_u32)part_count * 6u + (skip_byte ? 2u : 0u);
        if (mv_stride != (re1c_u32)frame_size) c->mv_stride_quirks++;
    }

    for (i = 0u; i < (re1c_u32)part_count; ++i) {
        const re1c_u8 *p = emr + 8u + i * 6u;
        re1c_pivot pv;
        re1c_vec3_centi rg, cg;
        re1c_i16 rp;
        re1c_u16 cc;
        re1c_u16 ref_cc = rd16(emr + part_off + i * 4u);
        re1c_u16 rel = rd16(emr + part_off + i * 4u + 2u);
        rc = re1c_get_pivot(view, bank, i, &pv); if (rc != RE1C_OK) return 0;
        if (!ref_parent(emr,emr_size,part_off,part_count,i,&rp)) return 0;
        if (!check_i32(c,"pivot.x",mv_pos_centi(rd16(p+4u)),pv.x_centi)) return 0;
        if (!check_i32(c,"pivot.y",mv_pos_centi(rd16(p+2u)),pv.y_centi)) return 0;
        if (!check_i32(c,"pivot.z",mv_pos_centi(rd16(p+0u)),pv.z_centi)) return 0;
        if (!check_i32(c,"pivot.parent",rp,pv.parent)) return 0;
        rc = re1c_get_child_count(view,bank,i,&cc); if (rc != RE1C_OK) return 0;
        if (!check_u32(c,"child_count",ref_cc,cc)) return 0;
        for (j=0u;j<(re1c_u32)ref_cc;++j) {
            re1c_u8 ch;
            rc = re1c_get_child(view,bank,i,j,&ch); if (rc != RE1C_OK) return 0;
            if (!check_u32(c,"child",emr[part_off+rel+j],ch)) return 0;
            c->children++;
        }
        if (!ref_global(emr,emr_size,part_off,part_count,i,&rg)) return 0;
        rc = re1c_get_bind_global_centi(view,bank,i,&cg); if (rc != RE1C_OK) return 0;
        if (!check_i32(c,"global.x",rg.x,cg.x)) return 0;
        if (!check_i32(c,"global.y",rg.y,cg.y)) return 0;
        if (!check_i32(c,"global.z",rg.z,cg.z)) return 0;
        c->parts++;
    }

    for (i=0u;i<clip_count;++i) {
        re1c_clip cl;
        re1c_u16 fc = rd16(edd + i*4u);
        re1c_u16 so = rd16(edd + i*4u + 2u);
        re1c_u32 ms;
        rc = re1c_get_clip(view,bank,i,&cl); if (rc != RE1C_OK) return 0;
        if (!check_u32(c,"clip.frames",fc,cl.frame_count)) return 0;
        if (!check_u32(c,"clip.offset",so,cl.stream_offset)) return 0;
        rc = re1c_get_clip_duration_ms(view,bank,i,&ms); if (rc != RE1C_OK) return 0;
        if (!check_u32(c,"clip.duration_ms",(re1c_u32)fc*40u,ms)) return 0;
        for (j=0u;j<(re1c_u32)fc;++j) {
            re1c_frame_ref fr;
            re1c_u32 pos=(re1c_u32)so+j*4u;
            if (!range_ok(pos,4u,edd_size)) return 0;
            rc = re1c_get_frame_ref(view,bank,i,j,&fr); if (rc != RE1C_OK) return 0;
            if (!check_u32(c,"frame.pose",rd16(edd+pos),fr.pose_index)) return 0;
            if (!check_u32(c,"frame.unknown",rd16(edd+pos+2u),fr.unknown_word)) return 0;
            if (!check_u32(c,"frame.alias",fr.unknown_word,fr.step_units)) return 0;
            c->frame_refs++;
        }
        c->clips++;
    }

    for (i=0u;i<pose_count;++i) {
        re1c_u32 pos=(re1c_u32)anim_off+i*(re1c_u32)frame_size;
        re1c_pose_root root;
        rc = re1c_get_pose_root(view,bank,i,&root); if (rc != RE1C_OK) return 0;
        if (!check_i32(c,"root.x",mv_pos_centi(rd16(emr+pos+4u)),root.x_centi)) return 0;
        if (!check_i32(c,"root.y",mv_pos_centi(rd16(emr+pos+2u)),root.y_centi)) return 0;
        if (!check_i32(c,"root.z",mv_pos_centi(rd16(emr+pos+0u)),root.z_centi)) return 0;
        for (j=0u;j<(re1c_u32)part_count;++j) {
            re1c_u32 aoff=pos+12u+j*6u;
            re1c_u16 ax=(re1c_u16)(rd16(emr+aoff+0u)&0x0fffu);
            re1c_u16 ay=(re1c_u16)(rd16(emr+aoff+2u)&0x0fffu);
            re1c_u16 az=(re1c_u16)(rd16(emr+aoff+4u)&0x0fffu);
            re1c_pose_angle a;
            re1c_i32 tq;
            rc = re1c_get_pose_angle(view,bank,i,j,&a); if (rc != RE1C_OK) return 0;
            if (!check_u32(c,"angle.ax",ax,a.file_ax12)) return 0;
            if (!check_u32(c,"angle.ay",ay,a.file_ay12)) return 0;
            if (!check_u32(c,"angle.az",az,a.file_az12)) return 0;
            if (!check_i32(c,"angle.rx",-(re1c_i32)az,a.render_x12)) return 0;
            if (!check_i32(c,"angle.ry",-(re1c_i32)ay,a.render_y12)) return 0;
            if (!check_i32(c,"angle.rz",-(re1c_i32)ax,a.render_z12)) return 0;
            tq=re1c_angle12_to_turn_q16((re1c_i16)ax);
            if (!check_i32(c,"angle.mv_q16",(re1c_i32)(((re1c_i32)ax*RE1C_Q16_ONE)/4095),tq)) return 0;
            c->angles++;
        }
        c->poses++;
    }
    return 1;
}

static int audit_tmd(mv_counts *c, const re1c_view *view,
                     const re1c_u8 *tmd, re1c_u32 tmd_size)
{
    re1c_u32 object_count, oi;
    int rc;
    if (tmd_size < 12u) return 0;
    object_count=rd32(tmd+8u);
    {
        re1c_u32 got;
        rc=re1c_get_tmd_object_count(view,&got); if(rc!=RE1C_OK)return 0;
        if(!check_u32(c,"tmd.object_count",object_count,got))return 0;
    }
    for(oi=0u;oi<object_count;++oi){
        re1c_u32 d=12u+oi*28u;
        re1c_tmd_object o;
        re1c_u32 vi,ni,pi,pos;
        re1c_u32 vo=rd32(tmd+d+0u),vc=rd32(tmd+d+4u);
        re1c_u32 no=rd32(tmd+d+8u),nc=rd32(tmd+d+12u);
        re1c_u32 po=rd32(tmd+d+16u),pc=rd32(tmd+d+20u),sc=rd32(tmd+d+24u);
        rc=re1c_get_tmd_object(view,oi,&o); if(rc!=RE1C_OK)return 0;
        if(!check_u32(c,"obj.vo",vo,o.vertex_offset))return 0;
        if(!check_u32(c,"obj.vc",vc,o.vertex_count))return 0;
        if(!check_u32(c,"obj.no",no,o.normal_offset))return 0;
        if(!check_u32(c,"obj.nc",nc,o.normal_count))return 0;
        if(!check_u32(c,"obj.po",po,o.primitive_offset))return 0;
        if(!check_u32(c,"obj.pc",pc,o.primitive_count))return 0;
        if(!check_u32(c,"obj.scale",sc,o.scale_word))return 0;
        for(vi=0u;vi<vc;++vi){
            re1c_vertex_raw v;
            const re1c_u8 *p=tmd+12u+vo+vi*8u;
            rc=re1c_get_vertex_raw(view,oi,vi,&v); if(rc!=RE1C_OK)return 0;
            if(!check_u32(c,"vx",rd16(p),v.file_x))return 0;
            if(!check_u32(c,"vy",rd16(p+2u),v.file_y))return 0;
            if(!check_u32(c,"vz",rd16(p+4u),v.file_z))return 0;
            if(!check_u32(c,"vpad",rd16(p+6u),v.pad))return 0;
            c->vertices++;
        }
        for(ni=0u;ni<nc;++ni){
            re1c_vertex_raw n;
            const re1c_u8 *p=tmd+12u+no+ni*8u;
            rc=re1c_get_normal_raw(view,oi,ni,&n); if(rc!=RE1C_OK)return 0;
            if(!check_u32(c,"nx",rd16(p),n.file_x))return 0;
            if(!check_u32(c,"ny",rd16(p+2u),n.file_y))return 0;
            if(!check_u32(c,"nz",rd16(p+4u),n.file_z))return 0;
            if(!check_u32(c,"npad",rd16(p+6u),n.pad))return 0;
            c->normals++;
        }
        pos=12u+po;
        for(pi=0u;pi<pc;++pi){
            re1c_u32 packet_size;
            re1c_u32 code;
            re1c_primitive pr;
            if(!range_ok(pos,4u,tmd_size))return 0;
            packet_size=4u+(re1c_u32)tmd[pos+1u]*4u;
            if(!range_ok(pos,packet_size,tmd_size))return 0;
            code=rd32(tmd+pos);
            rc=re1c_get_primitive(view,oi,pi,&pr);
            if(code==RE1C_PRIM_FT3_A || code==RE1C_PRIM_FT3_B || code==RE1C_PRIM_FT3_C ||
               code==RE1C_PRIM_FT4_A || code==RE1C_PRIM_FT4_B || code==RE1C_PRIM_FT4_C){
                if(rc!=RE1C_OK)return 0;
            } else if(rc!=RE1C_ERR_UNSUPPORTED) return 0;
            if(!check_u32(c,"prim.code",code,pr.code))return 0;
            if(!check_u32(c,"prim.size",packet_size,pr.raw_packet_size))return 0;
            c->checks++;
            c->raw_bytes+=packet_size;
            if(memcmp(tmd+pos,pr.raw_packet,(size_t)packet_size)!=0){fprintf(stderr,"MISMATCH prim raw\n");return 0;}
            if(code==RE1C_PRIM_FT3_A){
                if(!check_u32(c,"ft3.sides",3u,pr.sides))return 0;
                if(!check_u32(c,"ft3.u0",tmd[pos+4u],pr.u[0]))return 0;
                if(!check_u32(c,"ft3.v0",tmd[pos+5u],pr.v[0]))return 0;
                if(!check_u32(c,"ft3.cba",rd16(tmd+pos+6u),pr.cba))return 0;
                if(!check_u32(c,"ft3.u1",tmd[pos+8u],pr.u[1]))return 0;
                if(!check_u32(c,"ft3.v1",tmd[pos+9u],pr.v[1]))return 0;
                if(!check_u32(c,"ft3.tsb",rd16(tmd+pos+10u),pr.tsb))return 0;
                if(!check_u32(c,"ft3.u2",tmd[pos+12u],pr.u[2]))return 0;
                if(!check_u32(c,"ft3.v2",tmd[pos+13u],pr.v[2]))return 0;
                if(!check_u32(c,"ft3.n0",rd16(tmd+pos+16u),pr.normal_index[0]))return 0;
                if(!check_u32(c,"ft3.p0",rd16(tmd+pos+18u),pr.vertex_index[0]))return 0;
                if(!check_u32(c,"ft3.n1",rd16(tmd+pos+20u),pr.normal_index[1]))return 0;
                if(!check_u32(c,"ft3.p1",rd16(tmd+pos+22u),pr.vertex_index[1]))return 0;
                if(!check_u32(c,"ft3.n2",rd16(tmd+pos+24u),pr.normal_index[2]))return 0;
                if(!check_u32(c,"ft3.p2",rd16(tmd+pos+26u),pr.vertex_index[2]))return 0;
            }
            pos+=packet_size;
            c->primitives++;
        }
        c->objects++;
    }
    return 1;
}

static void print_pass(const char *path, const char *kind, const mv_counts *c, int alpha)
{
    printf("PASS %s kind=%s checks=%lu raw_bytes=%lu banks=%lu parts=%lu children=%lu clips=%lu frame_refs=%lu poses=%lu angles=%lu objects=%lu vertices=%lu normals=%lu primitives=%lu mv_stride_quirks=%lu alpha=%d\n",
           path, kind, (unsigned long)c->checks, (unsigned long)c->raw_bytes,
           (unsigned long)c->banks, (unsigned long)c->parts, (unsigned long)c->children,
           (unsigned long)c->clips, (unsigned long)c->frame_refs, (unsigned long)c->poses,
           (unsigned long)c->angles, (unsigned long)c->objects, (unsigned long)c->vertices,
           (unsigned long)c->normals, (unsigned long)c->primitives,
           (unsigned long)c->mv_stride_quirks, alpha);
}

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    re1c_u32 size;
    re1c_view view;
    re1c_u32 kind;
    re1c_u32 frame_ms;
    mv_counts c;
    int rc;
    if(argc!=2){fprintf(stderr,"usage: re1codec_mv_audit model.emd|model.emw|model.tmd\n");return 2;}
    memset(&c,0,sizeof(c));
    f=fopen(argv[1],"rb"); if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 2;}
    n=ftell(f); if(n<0 || (unsigned long)n>MV_MAX_FILE){fclose(f);return 2;}
    if(fseek(f,0,SEEK_SET)!=0){fclose(f);return 2;}
    size=(re1c_u32)n;
    if(fread(g_file,1,(size_t)size,f)!=(size_t)size){fclose(f);return 2;}
    fclose(f);

    rc=re1c_open_model(g_file,size,&view,&kind);
    if(rc!=RE1C_OK){fprintf(stderr,"codec open rc=%d\n",rc);return 1;}

    if(kind==RE1C_MODEL_TMD){
        if(!compare_section(&c,"TMD",&view.section[RE1C_SEC_TMD],g_file,0u,size))return 1;
        if(!audit_tmd(&c,&view,g_file,size))return 1;
        print_pass(argv[1],"TMD",&c,0);
        return 0;
    }

    if(kind==RE1C_MODEL_EMW){
        re1c_u32 emr_end;
        re1c_u32 edd_end;
        if(size<8u)return 1;
        emr_end=rd32(g_file+size-8u);
        edd_end=rd32(g_file+size-4u);
        if(emr_end==0u || emr_end>=edd_end || edd_end>=size-8u)return 1;
        if(!compare_section(&c,"EMR_B",&view.section[RE1C_SEC_EMR_B],g_file,0u,emr_end))return 1;
        if(!compare_section(&c,"EDD_B",&view.section[RE1C_SEC_EDD_B],g_file,emr_end,edd_end-emr_end))return 1;
        if(!compare_section(&c,"TMD",&view.section[RE1C_SEC_TMD],g_file,edd_end,size-8u-edd_end))return 1;
        if(!audit_bank(&c,&view,RE1C_BANK_B,g_file,emr_end,g_file+emr_end,edd_end-emr_end))return 1;
        if(!audit_tmd(&c,&view,g_file+edd_end,size-8u-edd_end))return 1;
        rc=re1c_get_mv_frame_duration_ms(&frame_ms); if(rc!=RE1C_OK)return 1;
        if(!check_u32(&c,"Timer4.Interval",40u,frame_ms))return 1;
        print_pass(argv[1],"EMW",&c,0);
        return 0;
    }

    if(kind==RE1C_MODEL_EMD){
        re1c_u32 off[5];
        re1c_u32 a1,t0,t1,im0,im1;
        int alpha;
        if(size<20u){fprintf(stderr,"too small\n");return 2;}
        off[0]=rd32(g_file+size-20u); off[1]=rd32(g_file+size-16u);
        off[2]=rd32(g_file+size-12u); off[3]=rd32(g_file+size-8u); off[4]=rd32(g_file+size-4u);
        alpha=mv_alpha(g_file,size);
        if(alpha){off[0]=0u;off[1]=0u;}
        a1=(off[0]>0u && off[0]<size)?off[0]:0u;
        im0=off[1]; im1=off[2];
        t0=off[2]; t1=off[3];
        if(off[1]>size || off[2]>size || off[3]>size || off[4]>size ||
           off[2]<off[1] || off[3]<off[2] || off[4]<off[3]){
            fprintf(stderr,"bad MV offsets\n");return 1;
        }
        if(a1==0u){
            if(!compare_section(&c,"EMR_A",&view.section[RE1C_SEC_EMR_A],g_file,0u,0u))return 1;
            if(!compare_section(&c,"EDD_A",&view.section[RE1C_SEC_EDD_A],g_file,0u,0u))return 1;
        }else{
            if(!compare_section(&c,"EMR_A",&view.section[RE1C_SEC_EMR_A],g_file,0u,off[0]))return 1;
            if(!compare_section(&c,"EDD_A",&view.section[RE1C_SEC_EDD_A],g_file,off[0],off[1]-off[0]))return 1;
        }
        if(!compare_section(&c,"EMR_B",&view.section[RE1C_SEC_EMR_B],g_file,im0,im1-im0))return 1;
        if(!compare_section(&c,"EDD_B",&view.section[RE1C_SEC_EDD_B],g_file,t0,t1-t0))return 1;
        if(!compare_section(&c,"TMD",&view.section[RE1C_SEC_TMD],g_file,off[3],off[4]-off[3]))return 1;
        if(size<20u || off[4]>size-20u)return 1;
        if(!compare_section(&c,"TIM",&view.section[RE1C_SEC_TIM],g_file,off[4],size-20u-off[4]))return 1;
        if(a1!=0u){
            if(!audit_bank(&c,&view,RE1C_BANK_A,g_file,off[0],g_file+off[0],off[1]-off[0]))return 1;
        }
        if(!audit_bank(&c,&view,RE1C_BANK_B,g_file+off[1],off[2]-off[1],g_file+off[2],off[3]-off[2]))return 1;
        if(!audit_tmd(&c,&view,g_file+off[3],off[4]-off[3]))return 1;
        rc=re1c_get_mv_frame_duration_ms(&frame_ms); if(rc!=RE1C_OK)return 1;
        if(!check_u32(&c,"Timer4.Interval",40u,frame_ms))return 1;
        print_pass(argv[1],"EMD",&c,alpha);
        return 0;
    }

    fprintf(stderr,"unsupported model kind\n");
    return 1;
}
