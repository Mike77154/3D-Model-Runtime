#include "ss89.h"
#include <string.h>
#include <stdlib.h>

/* All offsets and chunk lengths bounded to signed 31-bit ftell/fseek domain.
   We intentionally do not support >2 GiB file positions in this C89 variant. */

static int ss_fail(SS89_Model *m, int err, const char *where)
{
    m->last_error = err;
    strncpy(m->error_at, where, sizeof(m->error_at)-1U);
    m->error_at[sizeof(m->error_at)-1U] = '\0';
    return err;
}
static int ss_tell(SS89_Model *m, SS89_u32 *pos)
{
    long v = ftell(m->fp);
    if (v < 0L || (unsigned long)v > 0x7fffffffUL) return SS89_ERR_IO;
    *pos = (SS89_u32)v;
    return SS89_OK;
}
static int ss_at(SS89_Model *m, SS89_u32 pos)
{
    if (pos > m->file_bytes || pos > 0x7fffffffU) return SS89_ERR_RANGE;
    return fseek(m->fp, (long)pos, SEEK_SET) ? SS89_ERR_IO : SS89_OK;
}
static int ss_skip(SS89_Model *m, SS89_u32 amount)
{
    SS89_u32 now;
    if (ss_tell(m,&now)) return SS89_ERR_IO;
    if (amount > m->file_bytes-now) return SS89_ERR_IO;
    return ss_at(m,now+amount);
}
static int ss_read(SS89_Model *m, void *out, SS89_u32 len)
{
    SS89_u32 now;
    if (ss_tell(m,&now)) return SS89_ERR_IO;
    if (len > m->file_bytes-now) return SS89_ERR_IO;
    return fread(out, 1U, (size_t)len, m->fp) == (size_t)len ? SS89_OK : SS89_ERR_IO;
}
static int ss_u32(SS89_Model *m, SS89_u32 *out)
{
    SS89_u8 b[4];
    if (ss_read(m,b,4U)) return SS89_ERR_IO;
    *out = (SS89_u32)b[0] | ((SS89_u32)b[1]<<8) |
           ((SS89_u32)b[2]<<16) | ((SS89_u32)b[3]<<24);
    return SS89_OK;
}
static int ss_i32(SS89_Model *m, SS89_i32 *out)
{
    SS89_u32 v;
    if (ss_u32(m,&v)) return SS89_ERR_IO;
    /* C89 unsigned->signed conversion is implementation-defined for 2^31+.
       Reconstruct two's-complement value with only signed-safe arithmetic. */
    if (v<=2147483647U) *out=(SS89_i32)v;
    else *out=-1-(SS89_i32)(~v);
    return SS89_OK;
}
static int ss_tag(SS89_Model *m, const char *tag)
{
    char found[4];
    if (ss_read(m,found,4U)) return SS89_ERR_IO;
    return memcmp(found,tag,4U)==0 ? SS89_OK : SS89_ERR_FORMAT;
}
static int ss_peek_tag(SS89_Model *m, char out[4])
{
    SS89_u32 pos;
    if (ss_tell(m,&pos) || ss_read(m,out,4U) || ss_at(m,pos)) return SS89_ERR_IO;
    return SS89_OK;
}
static int ss_bytes_mul(SS89_u32 a, SS89_u32 b, SS89_u32 *out)
{
    if (b && a > 0x7fffffffU/b) return SS89_ERR_OVERFLOW;
    *out = a*b;
    return SS89_OK;
}
static int ss_chunk(SS89_Model *m, const char *tag, SS89_u32 bytes)
{
    SS89_u32 size;
    if (ss_tag(m,tag) || ss_u32(m,&size)) return SS89_ERR_FORMAT;
    if (size != bytes) return SS89_ERR_FORMAT;
    return SS89_OK;
}
static int ss_skip_chunk(SS89_Model *m, const char *tag, SS89_u32 bytes)
{
    if (ss_chunk(m,tag,bytes)) return SS89_ERR_FORMAT;
    return ss_skip(m,bytes);
}
static int ss_string(SS89_Model *m, char *dst, SS89_u32 capacity)
{
    SS89_u32 n, use;
    if (ss_u32(m,&n)) return SS89_ERR_IO;
    if (n > 1048576U) return SS89_ERR_RANGE;
    if (dst && capacity) {
        use=n;
        if (use>=capacity) use=capacity-1U;
        if (ss_read(m,dst,use)) return SS89_ERR_IO;
        dst[use]='\0';
        return ss_skip(m,n-use);
    }
    return ss_skip(m,n);
}
static SS89_i32 ss_bits_fixed(SS89_u32 bits)
{
    SS89_u32 ex=(bits>>23)&255U, mant, mag;
    int sh;
    if (!ex) return 0;
    if (ex==255U) return (bits&0x80000000U) ? (-2147483647-1) : 2147483647;
    mant=(bits&0x7fffffU)|0x800000U;
    sh=(int)ex-134;
    if (sh < -24) return 0;
    if (sh < 0) mag=mant>>(unsigned)(-sh);
    else if (sh>7 || mant > (2147483647U>>(unsigned)sh))
        mag=2147483647U;
    else mag=mant<<(unsigned)sh;
    if (bits&0x80000000U) return -(SS89_i32)mag;
    return (SS89_i32)mag;
}
static int ss_floatq(SS89_Model *m, SS89_i32 *out)
{
    SS89_u32 bits;
    if (ss_u32(m,&bits)) return SS89_ERR_IO;
    if (((bits>>23)&255U)==255U) return SS89_ERR_FORMAT;
    *out=ss_bits_fixed(bits);
    return SS89_OK;
}
static int ss_vec3q(SS89_Model *m, SS89_i32 p[3])
{
    int k;
    for (k=0;k<3;k++) if (ss_floatq(m,p+k)) return SS89_ERR_IO;
    return SS89_OK;
}
static int ss_limit(SS89_u32 n, SS89_u32 max)
{
    return n<=max ? SS89_OK : SS89_ERR_CAPACITY;
}
static int ss_skip_counts(SS89_Model *m, SS89_u32 count, SS89_u32 stride)
{
    SS89_u32 bytes;
    if (ss_bytes_mul(count,stride,&bytes)) return SS89_ERR_OVERFLOW;
    return ss_skip(m,bytes);
}

static int ss_one_mip(SS89_Model *m, unsigned target, int store)
{
    SS89_u32 polygonct, uvct, surfaces, patches, p, n, k, v, t;
    SS89_u32 ids[SS89_MAX_POLY_CORNERS], uvs[SS89_MAX_POLY_CORNERS];
    SS89_u32 render_flags, color, surface;
    SS89_u32 bytes, chunk_bytes, polys_in_surface, tex_in_surface;
    SS89_u32 tri_count=0U;
    char tag[4];
    int rc;
    rc=ss_chunk(m,"IPOL",4U); if(rc) return rc;
    if(ss_u32(m,&polygonct)) return SS89_ERR_IO;
    if (polygonct > 1000000U) return SS89_ERR_RANGE;
    m->mip_polygons[target]=polygonct;
    for (p=0U;p<polygonct;p++) {
        if (ss_peek_tag(m,tag)) return SS89_ERR_IO;
        if (!memcmp(tag,"MDP2",4U)) {
            if(ss_tag(m,"MDP2")||ss_u32(m,&n)) return SS89_ERR_FORMAT;
        } else if (!memcmp(tag,"MDPL",4U)) {
            if(ss_tag(m,"MDPL")||ss_chunk(m,"IMPV",4U)||ss_u32(m,&n)) return SS89_ERR_FORMAT;
        } else return SS89_ERR_FORMAT;
        if(n>SS89_MAX_POLY_CORNERS) return SS89_ERR_RANGE;
        for (k=0U;k<n;k++) {
            if(ss_u32(m,&v) || ss_u32(m,&t)) return SS89_ERR_IO;
            if (v>=m->vertices) return SS89_ERR_RANGE;
            if (store) { ids[k]=v; uvs[k]=t; }
        }
        if(ss_u32(m,&render_flags) || ss_u32(m,&color) || ss_u32(m,&surface)) return SS89_ERR_IO;
        if (!memcmp(tag,"MDPL",4U) && ss_skip(m,8U)) return SS89_ERR_IO;
        if (n>=3U && n-2U > 0x7fffffffU-tri_count) return SS89_ERR_OVERFLOW;
        if (store) {
            if(n>=3U && tri_count + n-2U>SS89_MAX_TRIANGLES) return SS89_ERR_CAPACITY;
            for(k=1U;k+1U<n;k++) {
                SS89_Triangle *tri=&m->triangles[tri_count++];
                tri->v[0]=ids[0]; tri->v[1]=ids[k];tri->v[2]=ids[k+1U];
                tri->uv[0]=uvs[0];tri->uv[1]=uvs[k];tri->uv[2]=uvs[k+1U];
                tri->surface=surface; tri->color=color;
            }
        } else if(n>=3U) tri_count += n-2U;
        (void)render_flags;
    }
    m->mip_triangles[target]=tri_count;
    if(ss_u32(m,&uvct)) return SS89_ERR_IO;
    if(uvct>1000000U) return SS89_ERR_RANGE;
    if (store && ss_limit(uvct,SS89_MAX_UV)) return SS89_ERR_CAPACITY;
    if(ss_peek_tag(m,tag)) return SS89_ERR_IO;
    if (!memcmp(tag,"TXV2",4U)) {
        if(ss_tag(m,"TXV2") || ss_u32(m,&bytes)) return SS89_ERR_FORMAT;
        if(ss_bytes_mul(uvct,52U,&chunk_bytes) || bytes!=chunk_bytes) return SS89_ERR_FORMAT;
        for(k=0;k<uvct;k++) {
            /* float3 UVW, MEX2D, BOOL, INDEX, float3 tangents U,V */
            if (ss_skip(m,12U)) return SS89_ERR_IO;
            if (store) {
                SS89_i32 u,vv;
                if (ss_i32(m,&u) || ss_i32(m,&vv)) return SS89_ERR_IO;
                m->texvertices[k].u_mex=u; m->texvertices[k].v_mex=vv;
                if(ss_skip(m,4U)||ss_u32(m,&m->texvertices[k].transformed_vertex)) return SS89_ERR_IO;
                if(m->texvertices[k].transformed_vertex>=m->vertices) return SS89_ERR_RANGE;
            } else if (ss_skip(m,16U)) return SS89_ERR_IO;
            if(ss_skip(m,24U)) return SS89_ERR_IO;
        }
    } else if (!memcmp(tag,"TXVT",4U)) {
        /* Legacy records: float3, MEX2D, BOOL and optionally INDEX. */
        if(ss_tag(m,"TXVT")||ss_u32(m,&bytes)) return SS89_ERR_FORMAT;
        if(ss_bytes_mul(uvct,m->version>=7U?28U:24U,&chunk_bytes) || bytes!=chunk_bytes)
            return SS89_ERR_FORMAT;
        for(k=0U;k<uvct;k++) {
            if(ss_skip(m,12U)) return SS89_ERR_IO;
            if(store){
                if(ss_i32(m,&m->texvertices[k].u_mex)||ss_i32(m,&m->texvertices[k].v_mex)) return SS89_ERR_IO;
            }else if(ss_skip(m,8U))return SS89_ERR_IO;
            if(ss_skip(m,4U)) return SS89_ERR_IO;
            if(m->version>=7U){
                if(store){
                    if(ss_u32(m,&m->texvertices[k].transformed_vertex))return SS89_ERR_IO;
                    if(m->texvertices[k].transformed_vertex>=m->vertices)return SS89_ERR_RANGE;
                }else if(ss_skip(m,4U))return SS89_ERR_IO;
            } else if(store) m->texvertices[k].transformed_vertex=0U;
        }
    } else return SS89_ERR_FORMAT;
    if(store) {
        m->uv_count=uvct;
        for(t=0U;t<tri_count;t++) {
            for(k=0;k<3U;k++) if(m->triangles[t].uv[k]>=uvct) return SS89_ERR_RANGE;
        }
    }
    if(ss_u32(m,&surfaces)) return SS89_ERR_IO;
    if(surfaces>65536U) return SS89_ERR_RANGE;
    if(store && ss_limit(surfaces,SS89_MAX_SURFACES))return SS89_ERR_CAPACITY;
    for (p=0U;p<surfaces;p++) {
        SS89_Surface *s=store?&m->surfaces[p]:NULL;
        char *dest=s?s->name:NULL;
        if (ss_string(m,dest,64U) || ss_skip(m,28U)) return SS89_ERR_IO;
        /* string, surface2DOffset float3, HPB float3, zoom float */
        if (m->version>=7U) {
            SS89_i32 shading, translucent;
            if(ss_i32(m,&shading)||ss_i32(m,&translucent)||ss_u32(m,&render_flags))return SS89_ERR_IO;
            if(s){s->shading=shading;s->translucency=translucent;s->flags=render_flags;}
            if(ss_u32(m,&polys_in_surface)) return SS89_ERR_IO;
            if(polys_in_surface>polygonct || ss_skip_counts(m,polys_in_surface,4U))return SS89_ERR_FORMAT;
            if(ss_u32(m,&tex_in_surface))return SS89_ERR_IO;
            if(tex_in_surface>uvct || ss_skip_counts(m,tex_in_surface,4U))return SS89_ERR_FORMAT;
            if(ss_u32(m,&color)) return SS89_ERR_IO;
            if(s)s->color=color;
        }
        if(m->version>=10U) {
            SS89_u32 co[6];
            for(k=0U;k<6U;k++) if(ss_u32(m,&co[k]))return SS89_ERR_IO;
            if(s){s->diffuse=co[0];s->reflection=co[1];s->specular=co[2];s->bump=co[3];}
        }
    }
    if(store){
        m->surface_count=surfaces;
        for(t=0;t<tri_count;t++)if(m->triangles[t].surface>=surfaces)return SS89_ERR_RANGE;
        m->triangles_count=tri_count;
    }
    /* Polygonal patches are present in V006 and later. */
    if(m->version>=6U){
        if(ss_skip(m,4U)||ss_u32(m,&patches))return SS89_ERR_IO;
        if(patches>SS89_MAX_PATCHES)return SS89_ERR_RANGE;
        for(p=0U;p<patches;p++){
            if(ss_u32(m,&n))return SS89_ERR_IO;
            if(n>polygonct) return SS89_ERR_RANGE;
            if(n){if(ss_skip_chunk(m,"OCPL",n*4U))return SS89_ERR_FORMAT;}
        }
    }
    return SS89_OK;
}

static int ss_parse(SS89_Model *m)
{
    SS89_u32 val, flags=0U, count, i, z, k, pos;
    SS89_u32 frame_bytes, bytes, n, ver_num, vertices, frames;
    char version_tag[4], tag[4];
    int rc;
    if (ss_tag(m,"MDAT"))return ss_fail(m,SS89_ERR_SIGNATURE,"MDAT");
    if(ss_read(m,version_tag,4U))return ss_fail(m,SS89_ERR_IO,"version");
    if(version_tag[0]!='V'||version_tag[1]!='0'||version_tag[2]!='0'||version_tag[3]<'2'||version_tag[3]>'9'){
        if(memcmp(version_tag,"V010",4U))return ss_fail(m,SS89_ERR_VERSION,"version");
        ver_num=10U;
    }else ver_num=(SS89_u32)(version_tag[3]-'0');
    m->version=(unsigned)ver_num;
    if(ver_num>=8U && ss_u32(m,&flags))return ss_fail(m,SS89_ERR_IO,"flags");
    m->flags=flags;
    if(ss_chunk(m,"IVTX",4U)||ss_u32(m,&vertices))return ss_fail(m,SS89_ERR_FORMAT,"IVTX");
    if(ss_chunk(m,"IFRM",4U)||ss_u32(m,&frames))return ss_fail(m,SS89_ERR_FORMAT,"IFRM");
    if(!vertices || !frames || vertices>SS89_MAX_VERTICES || frames>SS89_MAX_FRAMES)
        return ss_fail(m,SS89_ERR_CAPACITY,"vertices/frames");
    m->vertices=vertices;m->frames=frames;
    if(flags&SS89_MF_COMPRESSED_16BIT)m->frame_stride=8U;
    else m->frame_stride=4U;
    if(ss_bytes_mul(vertices,frames,&count)||ss_bytes_mul(count,m->frame_stride,&frame_bytes))
        return ss_fail(m,SS89_ERR_OVERFLOW,"frame bytes");
    if((flags&SS89_MF_COMPRESSED_16BIT)!=0U){
        if(ss_peek_tag(m,tag))return ss_fail(m,SS89_ERR_IO,"frame tag");
        if(memcmp(tag,"AV17",4U) && memcmp(tag,"AV16",4U))return ss_fail(m,SS89_ERR_FORMAT,"AV17/AV16");
        m->frame_legacy_normals=!memcmp(tag,"AV16",4U);
        if(ss_tag(m,tag)||ss_u32(m,&bytes) || bytes!=frame_bytes)return ss_fail(m,SS89_ERR_FORMAT,"frame data chunk");
    } else {
        if(ss_chunk(m,"AFVX",frame_bytes))return ss_fail(m,SS89_ERR_FORMAT,"AFVX");
    }
    if(ss_tell(m,&m->frame_pos) || ss_skip(m,frame_bytes)) return ss_fail(m,SS89_ERR_IO,"frame payload");
    if(ss_bytes_mul(frames,24U,&bytes) || ss_skip_chunk(m,"AFIN",bytes))return ss_fail(m,SS89_ERR_FORMAT,"AFIN");
    if(ss_bytes_mul(vertices,12U,&bytes) || ss_skip_chunk(m,"AMMV",bytes))return ss_fail(m,SS89_ERR_FORMAT,"AMMV");
    if(ss_bytes_mul(vertices,4U,&bytes)||ss_skip_chunk(m,"AVMK",bytes))return ss_fail(m,SS89_ERR_FORMAT,"AVMK");
    if(ss_chunk(m,"IMIP",4U)||ss_u32(m,&m->mips))return ss_fail(m,SS89_ERR_FORMAT,"IMIP");
    if(m->mips==0U||m->mips>SS89_MAX_MIPS)return ss_fail(m,SS89_ERR_RANGE,"mips");
    if(ss_chunk(m,"FMIP",SS89_MAX_MIPS*4U))return ss_fail(m,SS89_ERR_FORMAT,"FMIP");
    for(i=0U;i<SS89_MAX_MIPS;i++){
        SS89_u32 bits;
        if(ss_u32(m,&bits))return ss_fail(m,SS89_ERR_IO,"FMIP factor");
        /* Only the first md_MipCt slots are meaningful. Several genuine
           Croteam models contain uninitialized byte patterns (even NaNs)
           in the unused tail of the fixed-width FMIP array. */
        if(i<m->mips){
            if(((bits>>23)&255U)==255U)return ss_fail(m,SS89_ERR_FORMAT,"FMIP active factor");
            m->mip_switch_q16[i]=ss_bits_fixed(bits);
        }else m->mip_switch_q16[i]=0;
    }
    for(i=0;i<m->mips;i++){
        if(ss_tell(m,&m->mip_offsets[i]))return ss_fail(m,SS89_ERR_IO,"mip offset");
        rc=ss_one_mip(m,(unsigned)i,0);
        if(rc)return ss_fail(m,rc,"mip parse");
    }
    if(ss_peek_tag(m,tag))return ss_fail(m,SS89_ERR_IO,"patches tag");
    if(!memcmp(tag,"PTC2",4U)){
        if(ss_tag(m,"PTC2"))return ss_fail(m,SS89_ERR_FORMAT,"PTC2");
        for(i=0;i<SS89_MAX_PATCHES;i++){
            SS89_u32 plen, slen;
            if(ss_u32(m,&plen))return ss_fail(m,SS89_ERR_IO,"patch name len");
            if(plen>1048576U)return ss_fail(m,SS89_ERR_RANGE,"patch name len");
            if(plen)m->patches_present++;
            if(ss_skip(m,plen) || ss_tag(m,"DFNM") || ss_u32(m,&slen) || slen>1048576U || ss_skip(m,slen)||ss_skip(m,12U))
                return ss_fail(m,SS89_ERR_IO,"patch payload");
        }
    }else if(!memcmp(tag,"STMK",4U)){
        SS89_u32 mask;
        if(ss_chunk(m,"STMK",4U)||ss_u32(m,&mask))return ss_fail(m,SS89_ERR_FORMAT,"STMK");
        for(i=0;i<SS89_MAX_PATCHES;i++)if(mask&(1U<<i)){
            m->patches_present++;
            if(ss_tag(m,"DFNM") || ss_string(m,NULL,0U))return ss_fail(m,SS89_ERR_IO,"legacy patch");
        }
    } else return ss_fail(m,SS89_ERR_FORMAT,"patches tag");
    if(ss_chunk(m,"STXW",4U)||ss_i32(m,&m->texture_width))return ss_fail(m,SS89_ERR_FORMAT,"STXW");
    if(ss_chunk(m,"STXH",4U)||ss_i32(m,&m->texture_height))return ss_fail(m,SS89_ERR_FORMAT,"STXH");
    if(!memcmp(tag,"STMK",4U)){
        if(ss_tag(m,"POSS")||ss_u32(m,&val)||val!=SS89_MAX_PATCHES*8U||ss_skip(m,val))
            return ss_fail(m,SS89_ERR_FORMAT,"POSS");
    }
    if(ver_num<8U && ss_u32(m,&m->flags))return ss_fail(m,SS89_ERR_IO,"legacy flags");
    if(ss_skip(m,4U))return ss_fail(m,SS89_ERR_IO,"shadow quality");
    for(k=0U;k<3U;k++){
        if(ss_u32(m,&m->stretch_ieee[k]))return ss_fail(m,SS89_ERR_IO,"stretch bits");
        if(((m->stretch_ieee[k]>>23)&255U)==255U)return ss_fail(m,SS89_ERR_FORMAT,"stretch finite");
        m->stretch[k]=ss_bits_fixed(m->stretch_ieee[k]);
    }
    if(ver_num>=3U){
        if(ss_vec3q(m,m->center))return ss_fail(m,SS89_ERR_IO,"center");
    } else {m->center[0]=0;m->center[1]=0;m->center[2]=0;}
    if(ver_num>=4U){
        if(ss_u32(m,&m->collisions)||m->collisions>4096U)return ss_fail(m,SS89_ERR_RANGE,"collision count");
        for(i=0;i<m->collisions;i++){
            if(ss_skip(m,24U)||ss_string(m,NULL,0U))return ss_fail(m,SS89_ERR_IO,"collision");
        }
    } else {
        m->collisions=1U;
        if(ss_skip(m,24U))return ss_fail(m,SS89_ERR_IO,"old collision");
    }
    if(ss_peek_tag(m,tag))return ss_fail(m,SS89_ERR_IO,"collision type");
    if(!memcmp(tag,"COLI",4U) && (ss_tag(m,"COLI")||ss_skip(m,4U)))
        return ss_fail(m,SS89_ERR_IO,"COLI");
    if(ver_num>=5U){
        if(ss_u32(m,&m->attachments)||m->attachments>SS89_MAX_ATTACHMENTS)return ss_fail(m,SS89_ERR_CAPACITY,"attachments");
        for(i=0U;i<m->attachments;i++) {
            SS89_Attachment *at=&m->attachment_data[i];
            if(ss_u32(m,&at->center_vertex)||ss_u32(m,&at->front_vertex)||ss_u32(m,&at->up_vertex))return ss_fail(m,SS89_ERR_IO,"attachment indices");
            /* Some stock models contain unused/out-of-range attachment slots.
               Keep exact serialized IDs; validate only a slot being bound. */
            if(ss_vec3q(m,at->offset)||ss_vec3q(m,at->angles))return ss_fail(m,SS89_ERR_IO,"attachment placement");
        }
    }
    if(ss_chunk(m,"ICLN",4U)||ss_u32(m,&count)||count>SS89_MAX_COLORS)
        return ss_fail(m,SS89_ERR_FORMAT,"ICLN");
    for(i=0;i<count;i++){
        if(ss_u32(m,&val)||val>=SS89_MAX_COLORS || ss_string(m,NULL,0U))
            return ss_fail(m,SS89_ERR_FORMAT,"color name");
    }
    if(ss_tag(m,"ADAT")||ss_u32(m,&m->anim_count))return ss_fail(m,SS89_ERR_FORMAT,"ADAT");
    if(m->anim_count>SS89_MAX_ANIMS)return ss_fail(m,SS89_ERR_CAPACITY,"animations count");
    for(i=0;i<m->anim_count;i++){
        SS89_Animation *a=&m->anims[i];
        if(ss_read(m,a->name,32U))return ss_fail(m,SS89_ERR_IO,"animation name");
        a->name[32]='\0';
        for(k=0;k<32U;k++) if(!a->name[k]) break;
        a->name[k]='\0';
        if(ss_floatq(m,&a->seconds_per_frame_q16)||ss_u32(m,&n))return ss_fail(m,SS89_ERR_IO,"animation header");
        if(n>SS89_MAX_ANIM_KEYS-m->anim_keys_count)return ss_fail(m,SS89_ERR_CAPACITY,"animation keys");
        a->first_key=m->anim_keys_count;a->keys=n;
        for(z=0;z<n;z++){
            if(ss_u32(m,&val))return ss_fail(m,SS89_ERR_IO,"animation frame index");
            if(val>=m->frames)return ss_fail(m,SS89_ERR_RANGE,"animation frame index");
            m->anim_keys[m->anim_keys_count++]=val;
        }
    }
    if(ver_num>=10U){
        if(ss_u32(m,&m->model_diffuse))return ss_fail(m,SS89_ERR_IO,"diffuse model color");
    }
    if(ver_num>=9U){
        if(ss_skip(m,12U))return ss_fail(m,SS89_ERR_IO,"reflect/spec/bump colors");
    }
    if(ss_tell(m,&pos))return ss_fail(m,SS89_ERR_IO,"end offset");
    if(pos!=m->file_bytes)return ss_fail(m,SS89_ERR_FORMAT,"trailing bytes");
    rc=ss89_select_mip(m,0U);
    if(rc)return ss_fail(m,rc,"reload mip 0");
    return SS89_OK;
}

void ss89_close(SS89_Model *m)
{
    if(!m)return;
    if(m->fp) fclose(m->fp);
    /* Deliberately avoid mass memset: caller may reuse static storage. */
    m->fp=NULL;
}
int ss89_open(SS89_Model *m, const char *path)
{
    long len;
    int rc;
    if(!m||!path)return SS89_ERR_STATE;
    memset(m,0,sizeof(*m));
    m->fp=fopen(path,"rb");
    if(!m->fp)return ss_fail(m,SS89_ERR_IO,"fopen");
    if(fseek(m->fp,0L,SEEK_END))goto fail_io;
    len=ftell(m->fp);
    if(len<0L || (unsigned long)len>0x7fffffffUL)goto fail_io;
    m->file_bytes=(SS89_u32)len;
    if(fseek(m->fp,0L,SEEK_SET))goto fail_io;
    rc=ss_parse(m);
    if(rc){ss89_close(m);return rc;}
    return SS89_OK;
fail_io:
    ss89_close(m);
    return ss_fail(m,SS89_ERR_IO,"file length");
}
unsigned ss89_choose_mip(const SS89_Model *m, SS89_i32 factor)
{
    unsigned i;
    if(!m || m->mips==0U || m->mips>SS89_MAX_MIPS)return 0U;
    for(i=0U;i<m->mips;i++)if(factor<m->mip_switch_q16[i])return i;
    return m->mips-1U;
}
int ss89_select_mip(SS89_Model *m, unsigned mip)
{
    int rc;
    if(!m||!m->fp)return SS89_ERR_STATE;
    if(mip>=m->mips)return ss_fail(m,SS89_ERR_RANGE,"select mip");
    m->triangles_count=0U;m->uv_count=0U;m->surface_count=0U;
    if(ss_at(m,m->mip_offsets[mip]))return ss_fail(m,SS89_ERR_IO,"seek mip");
    rc=ss_one_mip(m,mip,1);
    if(rc)return ss_fail(m,rc,"load selected mip");
    m->selected_mip=mip;
    return SS89_OK;
}
static SS89_i32 ss_clamp_add(SS89_i32 a, SS89_i32 b)
{
    if(b>0 && a>2147483647-b)return 2147483647;
    if(b<0 && a<(-2147483647-1)-b)return -2147483647-1;
    return a+b;
}
/* Exact signed int16 * IEEE754 binary32 -> Q16.16, using two 32-bit limbs.
   No float, double, int64 or heap. Tiny scales are NOT rounded to zero
   before multiplication. The shift derives from 24-bit IEEE mantissa. */
static SS89_i32 ss_mul_frame_scale(SS89_i32 raw, SS89_u32 bits)
{
    SS89_u32 m, a, low, highpart, limb, hi, carry, val;
    SS89_u32 exponent=(bits>>23)&255U;
    int negative, shift;
    if(raw==0 || (bits&0x7fffffffU)==0U)return 0;
    negative=(raw<0)!=((bits&0x80000000U)!=0U);
    a=raw<0?(SS89_u32)(-raw):(SS89_u32)raw;
    if(exponent==0U) {m=bits&0x7fffffU;shift=133;}
    else {m=(bits&0x7fffffU)|0x800000U;shift=134-(int)exponent;}
    low=(m&65535U)*a;
    highpart=(m>>16)*a;
    limb=low+((highpart&65535U)<<16);
    carry=(limb<low)?1U:0U;
    hi=(highpart>>16)+carry;
    if(shift>=64)val=0U;
    else if(shift>=32)val=hi>>(unsigned)(shift-32);
    else if(shift>0) {
        if((hi>>(unsigned)shift)!=0U)return negative?(-2147483647-1):2147483647;
        val=(hi<<(unsigned)(32-shift))|(limb>>(unsigned)shift);
    }else if(shift==0){
        if(hi)return negative?(-2147483647-1):2147483647;
        val=limb;
    }else {
        if(hi || -shift>=32 || limb>(0x7fffffffU>>(unsigned)(-shift)))
            return negative?(-2147483647-1):2147483647;
        val=limb<<(unsigned)(-shift);
    }
    if(val>2147483647U) return negative?(-2147483647-1):2147483647;
    return negative?-(SS89_i32)val:(SS89_i32)val;
}
int ss89_decode_frame(SS89_Model *m, unsigned frame, SS89_Vertex *out, unsigned cap)
{
    SS89_u32 off, bytes, idx, i;
    SS89_u8 raw[8];
    SS89_i32 x,y,z;
    if(!m||!m->fp||!out)return SS89_ERR_STATE;
    if(frame>=m->frames || cap<m->vertices)return SS89_ERR_RANGE;
    if(ss_bytes_mul(m->vertices,m->frame_stride,&bytes)||ss_bytes_mul(frame,bytes,&off))
        return ss_fail(m,SS89_ERR_OVERFLOW,"frame seek");
    if(m->frame_pos>0x7fffffffU-off || ss_at(m,m->frame_pos+off))
        return ss_fail(m,SS89_ERR_IO,"frame seek");
    for(i=0U;i<m->vertices;i++){
        if(ss_read(m,raw,m->frame_stride))return ss_fail(m,SS89_ERR_IO,"read vertex");
        if(m->frame_stride==8U){
            idx=(SS89_u32)raw[0]|((SS89_u32)raw[1]<<8);
            x=(idx&0x8000U)?(SS89_i32)idx-65536:(SS89_i32)idx;
            idx=(SS89_u32)raw[2]|((SS89_u32)raw[3]<<8);
            y=(idx&0x8000U)?(SS89_i32)idx-65536:(SS89_i32)idx;
            idx=(SS89_u32)raw[4]|((SS89_u32)raw[5]<<8);
            z=(idx&0x8000U)?(SS89_i32)idx-65536:(SS89_i32)idx;
            out[i].norm0=raw[6];out[i].norm1=raw[7];
        }else{
            x=(raw[0]&128U)?(SS89_i32)raw[0]-256:(SS89_i32)raw[0];
            y=(raw[1]&128U)?(SS89_i32)raw[1]-256:(SS89_i32)raw[1];
            z=(raw[2]&128U)?(SS89_i32)raw[2]-256:(SS89_i32)raw[2];
            out[i].norm0=raw[3];out[i].norm1=0U;
        }
        /* UnpackVertex computes raw*stretch + center exactly (before
           presentation object-level transforms). All arithmetic saturated. */
        out[i].x=ss_clamp_add(ss_mul_frame_scale(x,m->stretch_ieee[0]),m->center[0]);
        out[i].y=ss_clamp_add(ss_mul_frame_scale(y,m->stretch_ieee[1]),m->center[1]);
        out[i].z=ss_clamp_add(ss_mul_frame_scale(z,m->stretch_ieee[2]),m->center[2]);
    }
    return SS89_OK;
}
int ss89_decode_anim_key(SS89_Model *m, unsigned anim, unsigned key,
                         SS89_Vertex *out, unsigned cap)
{
    SS89_Animation *a;
    if(!m||!m->fp)return SS89_ERR_STATE;
    if(anim>=m->anim_count)return SS89_ERR_RANGE;
    a=&m->anims[anim];
    if(key>=a->keys)return SS89_ERR_RANGE;
    return ss89_decode_frame(m,m->anim_keys[a->first_key+key],out,cap);
}
const char *ss89_error_string(int code)
{
    switch(code){
    case SS89_OK:return "ok";
    case SS89_ERR_IO:return "file I/O or truncation";
    case SS89_ERR_SIGNATURE:return "wrong signature (expected MDAT)";
    case SS89_ERR_VERSION:return "unsupported version (expected V002-V010)";
    case SS89_ERR_RANGE:return "invalid field/index/count";
    case SS89_ERR_FORMAT:return "malformed structure/chunk";
    case SS89_ERR_CAPACITY:return "fixed-capacity limit exceeded";
    case SS89_ERR_STATE:return "invalid model state";
    case SS89_ERR_OVERFLOW:return "arithmetic overflow";
    default:return "unknown error";
    }
}
/* Print fixed-point Q16.16 without float conversion. */
static void ss_print_q(FILE *f, SS89_i32 n)
{
    SS89_u32 mag;
    SS89_u32 whole, frac;
    if(n<0){fputc('-',f);mag=(SS89_u32)(-(n+1))+1U;}
    else mag=(SS89_u32)n;
    whole=mag>>16;
    frac=((mag&65535U)*1000U+32768U)>>16;
    if(frac==1000U){whole++;frac=0U;}
    fprintf(f,"%u.%03u",(unsigned)whole,(unsigned)frac);
}
static void ss_print_uv(FILE *f, SS89_i32 n, SS89_i32 denom)
{
    SS89_u32 mag, d, whole, rem, decimals;
    if(denom<=0 || denom>2000000){fputs("0.000",f);return;}
    if(n<0){fputc('-',f);mag=(SS89_u32)(-(n+1))+1U;}
    else mag=(SS89_u32)n;
    d=(SS89_u32)denom;
    whole=mag/d;rem=mag%d;
    decimals=(rem*1000U+d/2U)/d;
    if(decimals>=1000U){whole++;decimals=0U;}
    fprintf(f,"%u.%03u",(unsigned)whole,(unsigned)decimals);
}
int ss89_export_obj(SS89_Model *m, unsigned frame, const char *path,
                    SS89_Vertex *scratch, unsigned cap)
{
    FILE *fp;
    SS89_u32 i,j;
    int rc;
    if(!path)return SS89_ERR_STATE;
    rc=ss89_decode_frame(m,frame,scratch,cap);
    if(rc)return rc;
    fp=fopen(path,"wb");if(!fp)return SS89_ERR_IO;
    fprintf(fp,"# serious_model89 V%03u frame %u mip %u\n",m->version,frame,m->selected_mip);
    fprintf(fp,"# integer-only Q16.16 decode; fan triangulation\n");
    for(i=0;i<m->vertices;i++){
        fputs("v ",fp); ss_print_q(fp,scratch[i].x);fputc(' ',fp);
        ss_print_q(fp,scratch[i].y);fputc(' ',fp);
        ss_print_q(fp,scratch[i].z);fputc('\n',fp);
    }
    for(i=0;i<m->uv_count;i++){
        fputs("vt ",fp); ss_print_uv(fp,m->texvertices[i].u_mex,m->texture_width);
        fputc(' ',fp);
        /* OBJ's bottom-origin V must invert the TEX top-row raster mapping.
           Negative V is equivalent to 1-V when texture wrap is enabled. */
        if(m->texvertices[i].v_mex>0){fputc('-',fp);ss_print_uv(fp,m->texvertices[i].v_mex,m->texture_height);}
        else if(m->texvertices[i].v_mex<0)ss_print_uv(fp,-m->texvertices[i].v_mex,m->texture_height);
        else fputs("0.000",fp);
        fputc('\n',fp);
    }
    for(i=0;i<m->triangles_count;i++){
        SS89_Triangle *t=&m->triangles[i];
        if(i==0U || m->triangles[i-1U].surface!=t->surface){
            if(t->surface<m->surface_count && m->surfaces[t->surface].name[0]){
                fputs("g ",fp);
                for(j=0;m->surfaces[t->surface].name[j] && j<63U;j++){
                    char c=m->surfaces[t->surface].name[j];
                    fputc(c==' ' ? '_' : c,fp);
                }
                fputc('\n',fp);
            } else fprintf(fp,"g surface_%u\n",(unsigned)t->surface);
        }
        fprintf(fp,"f %u/%u %u/%u %u/%u\n",
            (unsigned)t->v[0]+1U,(unsigned)t->uv[0]+1U,
            (unsigned)t->v[1]+1U,(unsigned)t->uv[1]+1U,
            (unsigned)t->v[2]+1U,(unsigned)t->uv[2]+1U);
    }
    if(fclose(fp)!=0)return SS89_ERR_IO;
    return SS89_OK;
}
