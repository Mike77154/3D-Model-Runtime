/* Gun Survivor PC NAB/NAC, Tyrant HMD/TIM runtime bridge, v19 native rules.
 * White-room adaptation of the user's C89 v19 source. No heap, no floating
 * point and no game assets built into the library. Binary buffers live in the
 * caller-provided modelrt_handle arena; triangles are streamed by callbacks.
 * NAC timing/poses are not established: only native clip catalogs are offered,
 * marked UNBOUND, and animated visits deliberately refuse to synthesize them.
 */
#include "../modelrt_internal.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define GS_MAX_VERTS 2000000u
#define GS_MAX_PARTS 2048u
#define GS_MAX_COORDS 2048u
#define GS_MAX_TEXTURES 128u
#define GS_MAX_NAB_SIZE (100u*1024u*1024u)
#define GS_MAX_HMD_SIZE (8u*1024u*1024u)
#define GS_MAX_TRACKS 256u
#define GS_MAX_CLIPS 256u

#define GS_NAB 1
#define GS_HMD 2
#define GS_NAC 3

typedef struct gs_part_s {
    modelrt_u32 first, count, joint, texture;
} gs_part;
typedef struct gs_coord_s {
    modelrt_i32 r[16], t[16];
    modelrt_u32 parent;
} gs_coord;
typedef struct gs_tex_s {
    size_t pixels;
    modelrt_u32 w, h;
} gs_tex;
typedef struct gs_hpoly_s {
    modelrt_u32 offset;
    modelrt_u32 corners;
} gs_hpoly;
typedef struct gs_state_s {
    int kind;
    const modelrt_u8 *bytes;
    size_t length;
    modelrt_u32 nv, nt, np, nc, ntex;
    size_t vertices_offset;
    gs_part *parts;
    gs_coord *coords;
    gs_tex textures[GS_MAX_TEXTURES];
    modelrt_u32 clip_count, clip_tracks;
    modelrt_u32 *clip_keys;
    /* Special HMD branch */
    modelrt_u32 h_verts, h_norms, h_polygons, h_tris;
    size_t h_vert_off, h_norm_off, h_poly_off;
    gs_hpoly *hpolys;
    const modelrt_u8 *tim;
    size_t tim_size;
    modelrt_u32 tim_x, tim_y;
} gs_state;

static modelrt_u32 gs_u16(const modelrt_u8 *p)
{ return (modelrt_u32)p[0] | ((modelrt_u32)p[1] << 8); }
static modelrt_u32 gs_u32(const modelrt_u8 *p)
{ return gs_u16(p) | (gs_u16(p + 2) << 16); }
static int gs_range(size_t off, size_t count, size_t len)
{ return off <= len && count <= len - off; }
static int gs_read32(const modelrt_u8 *p, size_t sz, size_t at, modelrt_u32 *out)
{
    if (!gs_range(at, 4u, sz)) return 0;
    *out = gs_u32(p + at); return 1;
}
/* Exact C89 v19 software float32 -> signed Q16 conversion, never using float. */
static int gs_float_q16(modelrt_u32 bits, modelrt_i32 *out)
{
    modelrt_u32 exp, mant, mag, val;
    int shift, neg;
    neg = (bits & 0x80000000u) != 0u;
    mag = bits & 0x7fffffffu;
    exp = (mag >> 23) & 255u;
    mant = mag & 0x7fffffu;
    if (exp == 255u) return 0;
    if (exp == 0u) { *out = 0; return 1; }
    mant |= 0x800000u;
    shift = (int)exp - 134;
    if (shift >= 0) {
        if (shift > 7 || mant > 0x7fffffffu >> shift) return 0;
        val = mant << shift;
    } else {
        if (shift <= -32) { *out = 0; return 1; }
        val = (mant + (1u << (-shift-1))) >> (-shift);
    }
    if (val > 0x7fffffffu) return 0;
    *out = neg ? -(modelrt_i32)val : (modelrt_i32)val;
    return 1;
}
static modelrt_i32 gs_s16(modelrt_u32 u)
{ return u & 32768u ? (modelrt_i32)u - 65536 : (modelrt_i32)u; }
static int gs_add(modelrt_i32 a, modelrt_i32 b, modelrt_i32 *result)
{
    if ((b > 0 && a > 2147483647 - b) ||
        (b < 0 && a < (-2147483647 - 1) - b)) return 0;
    *result = a + b; return 1;
}
static int gs_mulq12(modelrt_i32 a, modelrt_i32 b, modelrt_i32 *out)
{
    modelrt_i32 whole, rem, v;
    if (b < -16384 || b > 16384) return 0;
    whole = a / 4096; rem = a % 4096;
    if (whole > 131071 || whole < -131071) return 0;
    v = whole * b;
    return gs_add(v, (rem * b) / 4096, out);
}
static int gs_apply(modelrt_i32 p[3], const modelrt_i32 mat[16], int translate)
{
    modelrt_i32 v[3], sum, t, tmp;
    modelrt_u32 col, row;
    for (col = 0u; col < 3u; ++col) {
        sum = 0;
        for (row = 0u; row < 3u; ++row) {
            if (!gs_mulq12(p[row], mat[row*4u+col], &t) ||
                !gs_add(sum, t, &tmp)) return 0;
            sum = tmp;
        }
        if (translate) {
            if (!gs_add(sum, mat[12u+col]/16, &tmp)) return 0;
            sum = tmp;
        }
        v[col] = sum;
    }
    for (col = 0u; col < 3u; ++col) p[col] = v[col];
    return 1;
}
/* Reproduce v19 native R @ T @ parent then source->Y-up axes. */
static int gs_world(gs_state *s, modelrt_i32 p[3], modelrt_u32 node, int normal)
{
    modelrt_u32 depth = 0u;
    modelrt_i32 x, y, z;
    if (node > s->nc) return 0;
    for (;;) {
        gs_coord *c = &s->coords[node];
        if (!gs_apply(p, c->r, 0) || !gs_apply(p, c->t, !normal)) return 0;
        if (node == 0u || c->parent == node) break;
        node = c->parent;
        if (node > s->nc || ++depth > s->nc + 1u) return 0;
    }
    x=p[0];y=p[1];z=p[2];
    if (x == (-2147483647 - 1) || y == (-2147483647 - 1)) return 0;
    p[0] = z; p[1] = -y; p[2] = -x;
    return 1;
}
/* C89 safe Q8 native to Q16 meters = (Q8*32)/125. */
static modelrt_q16 gs_meter(modelrt_i32 q)
{
    modelrt_i32 whole, rem, a, b;
    whole = q / 125; rem = q % 125;
    if (whole > 67108863) return 2147483647;
    if (whole < -67108863) return (-2147483647 - 1);
    a = whole * 32;
    b = (rem * 32) / 125;
    return a + b;
}
static modelrt_q16 gs_qnorm(modelrt_i32 q) { return q/16; }
static void gs_name(char *dst, size_t cap, const char *path)
{
    const char *base = path, *p;
    for (p = path; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    modelrt_copy_string(dst, cap, base);
}
static int gs_probe(const char *path, const modelrt_u8 *head, size_t n)
{
    modelrt_u32 a, b;
    if (modelrt_ext_eq(path, ".nac")) {
        if (n < 4u) return MODELRT_PROBE_MAYBE;
        a=gs_u32(head);
        return a>0u && a<=500000u ? MODELRT_PROBE_YES : MODELRT_PROBE_NO;
    }
    if (modelrt_ext_eq(path, ".nab")) {
        if (n < 8u) return MODELRT_PROBE_MAYBE;
        a=gs_u32(head); b=gs_u32(head+4);
        if (b > 0u && b <= GS_MAX_VERTS/3u && a == b*3u)
            return MODELRT_PROBE_YES;
        return MODELRT_PROBE_NO;
    }
    if (modelrt_ext_eq(path, ".hmd")) {
        if (n < 24u) return MODELRT_PROBE_MAYBE;
        return gs_u32(head) == 0x50u && gs_u32(head + 12u) == 3u
            ? MODELRT_PROBE_YES : MODELRT_PROBE_NO;
    }
    return MODELRT_PROBE_NO;
}
/* Source v19 walks image descriptors 0x160 and planes separately. */
static int gs_texture_header(const modelrt_u8 *d, size_t len, size_t at)
{
    modelrt_u32 sz,w,h;
    if (!gs_range(at,16u,len)) return 0;
    sz=gs_u32(d+at);w=gs_u32(d+at+8u);h=gs_u32(d+at+12u);
    return gs_u32(d+at+4u)==25u && w>0u && h>0u &&
           w<=2048u && h<=2048u && w<=2097152u/h &&
           sz==w*h*2u && gs_range(at+16u,sz,len);
}
static int gs_nab_open(modelrt_handle *h, gs_state *s)
{
    const modelrt_u8 *d=s->bytes;
    size_t n=s->length, at;
    modelrt_u32 objs, nd, plane_base[8], plane_count[8];
    modelrt_u32 i,j,nblocks,nb,refp,field[6],assoc,total,coords_count,idx;
    modelrt_u32 word;
    modelrt_i32 q;
    s->nv=gs_u32(d);s->nt=gs_u32(d+4u);
    if (!s->nt || s->nt>GS_MAX_VERTS/3u || s->nv!=s->nt*3u ||
        !gs_range(8u,(size_t)s->nv*36u,n)) return MODELRT_ERR_FORMAT;
    at=8u+(size_t)s->nv*36u;
    if (!gs_read32(d,n,at,&objs) || !objs || objs>8u) return MODELRT_ERR_FORMAT;
    at+=4u;nd=0u;s->ntex=0u;
    for (i=0u;i<8u;++i) {plane_base[i]=0u;plane_count[i]=0u;}
    for (;;) {
        if (gs_texture_header(d,n,at)) {
            modelrt_u32 sz=gs_u32(d+at), w=gs_u32(d+at+8u),ht=gs_u32(d+at+12u);
            if (!nd || s->ntex >= GS_MAX_TEXTURES) return MODELRT_ERR_FORMAT;
            s->textures[s->ntex].pixels=at+16u;
            s->textures[s->ntex].w=w;s->textures[s->ntex].h=ht;
            ++plane_count[nd-1u];++s->ntex;at+=16u+sz;
        } else if (nd < objs && gs_range(at,0x160u,n) &&
                   gs_texture_header(d,n,at+0x160u)) {
            at+=0x160u;plane_base[nd]=s->ntex;++nd;
        } else break;
    }
    if (nd!=objs || !s->ntex || !gs_read32(d,n,at,&s->np) ||
        !s->np || s->np>GS_MAX_PARTS) return MODELRT_ERR_FORMAT;
    at+=4u;
    s->parts=(gs_part*)modelrt_arena_alloc(h,(size_t)s->np*sizeof(gs_part),8u);
    if (!s->parts) return MODELRT_ERR_NO_SPACE;
    total=0u;
    for (i=0u;i<s->np;++i) {
        if (!gs_range(at,28u,n)) return MODELRT_ERR_FORMAT;
        for (j=0u;j<6u;++j) field[j]=gs_u32(d+at+4u*j);
        refp=field[0]!=0u?gs_u32(d+at+24u):0u;
        assoc=gs_u32(d+at+(field[0]!=0u?28u:24u));
        at+=field[0]!=0u?32u:28u;
        if (field[2]!=total || field[3]>s->nt-total) return MODELRT_ERR_FORMAT;
        total+=field[3];
        s->parts[i].first=field[2];s->parts[i].count=field[3];
        s->parts[i].joint=assoc;
        s->parts[i].texture=0xffffffffu;
        if (field[0]!=0u) {
            if (refp>=nd || field[5]>=plane_count[refp]) return MODELRT_ERR_FORMAT;
            s->parts[i].texture=plane_base[refp]+field[5];
        }
    }
    if (total!=s->nt || !gs_range(at,(size_t)s->nv*60u+(size_t)s->nt*20u,n))
        return MODELRT_ERR_FORMAT;
    at+=(size_t)s->nv*60u+(size_t)s->nt*20u;
    if (!gs_read32(d,n,at,&nblocks) || nblocks>GS_MAX_COORDS) return MODELRT_ERR_FORMAT;
    at+=4u;
    for (i=0u;i<nblocks;++i) {
        if (!gs_range(at,96u,n)) return MODELRT_ERR_FORMAT;
        nb=gs_u32(d+at+12u);at+=96u;
        if (nb>5000u || !gs_range(at,(size_t)nb*8u,n)) return MODELRT_ERR_FORMAT;
        at+=(size_t)nb*8u;
    }
    if (!gs_range(at,28u,n)) return MODELRT_ERR_FORMAT;
    at+=24u;coords_count=gs_u32(d+at);at+=4u;
    if (coords_count!=nblocks) return MODELRT_ERR_FORMAT;
    s->nc=coords_count;
    s->coords=(gs_coord*)modelrt_arena_alloc(h,((size_t)s->nc+1u)*sizeof(gs_coord),8u);
    if (!s->coords) return MODELRT_ERR_NO_SPACE;
    for (i=0u;i<=s->nc;++i) {
        if (!gs_range(at,264u,n)) return MODELRT_ERR_FORMAT;
        s->coords[i].parent=gs_u32(d+at+260u);
        if (s->coords[i].parent>s->nc) return MODELRT_ERR_FORMAT;
        for (j=0u;j<16u;++j) {
            word=gs_u32(d+at+j*4u);
            if (!gs_float_q16(word,&q)) return MODELRT_ERR_FORMAT;
            s->coords[i].r[j]=q/16;
            word=gs_u32(d+at+(32u+j)*4u);
            if (!gs_float_q16(word,&q)) return MODELRT_ERR_FORMAT;
            s->coords[i].t[j]=q/16;
        }
        at+=264u;
    }
    if (at!=n) return MODELRT_ERR_FORMAT;
    for (idx=0u;idx<s->np;++idx) if (s->parts[idx].joint>s->nc) return MODELRT_ERR_FORMAT;
    s->vertices_offset=8u;
    return MODELRT_OK;
}
/* NAC v19 validator with TC/no-TC grouping and deliberately no synthetic FPS.
 * Counts source key references per clip, separately from authored duration.
 */
static int gs_nac_parse(gs_state *s, const modelrt_u8 *d, size_t len,
                         modelrt_u32 tracks, modelrt_u32 *keys)
{
    modelrt_u32 recs,events,first,second,clips,t,c,at,tag,ref,start,tail,i;
    size_t base;
    int tc;
    if (len<20u || !tracks || tracks>GS_MAX_TRACKS) return 0;
    recs=gs_u32(d); if (!recs || recs>500000u ||
        (size_t)recs>(len-4u)/112u) return 0;
    base=4u+(size_t)recs*112u;
    if (!gs_range(base,4u,len))return 0;
    events=gs_u32(d+base);
    if (!events || events>800000u || (size_t)events>(len-base-4u)/8u ||
        base+4u+(size_t)events*8u!=len) return 0;
    base+=4u;
    first=gs_u32(d+base);second=gs_u32(d+base+4u);
    tc=(first&65535u)==0x4354u;
    if (tc) {
        clips=(first>>16)&255u;
        if (((first>>24)&255u)!=tracks || second!=recs-1u || !clips) return 0;
        at=1u;
    } else {
        clips=0u;at=0u;
        for (i=0u;i<events;++i) {
            tag=gs_u32(d+base+(size_t)i*8u);
            ref=gs_u32(d+base+(size_t)i*8u+4u);
            if (ref==0xffffffffu && (tag>>16)==0x8000u) ++clips;
        }
        if (!clips || clips%tracks) return 0;
        clips/=tracks;
    }
    if (clips>GS_MAX_CLIPS) return 0;
    for (i=0u;i<clips;++i)keys[i]=0u;
    for(t=0u;t<tracks;++t) {
        for(c=0u;c<clips;++c) {
            if (at>=events) return 0;
            tag=gs_u32(d+base+(size_t)at*8u);
            ref=gs_u32(d+base+(size_t)at*8u+4u);
            if (ref!=0xffffffffu || (tag>>16)!=0x8000u) return 0;
            start=at++;
            for (;;) {
                if (at>=events) return 0;
                tag=gs_u32(d+base+(size_t)at*8u);
                ref=gs_u32(d+base+(size_t)at*8u+4u);
                if (ref==0xffffffffu) break;
                if (ref>=recs) return 0;
                ++keys[c];++at;
            }
            if ((tag>>16)!=(0x8080u+c*0x80u) ||
                (tag&65535u)!=(tc?start:start+1u)) return 0;
            ++at;
        }
    }
    tail=tc?tracks*10u:tracks;
    if ((size_t)at+tail!=events) return 0;
    for (i=at;i<events;++i) {
        ref=gs_u32(d+base+(size_t)i*8u+4u);
        if(ref>=recs) return 0;
    }
    s->clip_count=clips;s->clip_tracks=tracks;
    return 1;
}
/* Standalone NAC animation source.  Expose only an unbound catalog. */
static int gs_nac_open(modelrt_handle *h, gs_state *s)
{
    modelrt_u32 tracks=0u,guess,keys[GS_MAX_CLIPS];
    const modelrt_u8 *d=s->bytes;
    size_t len=s->length;
    modelrt_u32 nr;
    size_t off;
    if(len<20u)return MODELRT_ERR_FORMAT;
    nr=gs_u32(d);
    if(nr && nr<=500000u && nr<=(len-4u)/112u){
        off=4u+(size_t)nr*112u;
        if(gs_range(off,12u,len)){
            modelrt_u32 tag=gs_u32(d+off+4u);
            if((tag&65535u)==0x4354u)tracks=(tag>>24)&255u;
        }
    }
    if(tracks)(void)gs_nac_parse(s,d,len,tracks,keys);
    else for(guess=1u;guess<=GS_MAX_TRACKS&&!s->clip_count;++guess)
        (void)gs_nac_parse(s,d,len,guess,keys);
    if(!s->clip_count)return MODELRT_ERR_FORMAT;
    s->clip_keys=(modelrt_u32*)modelrt_arena_alloc(h,(size_t)s->clip_count*sizeof(modelrt_u32),8u);
    if(!s->clip_keys)return MODELRT_ERR_NO_SPACE;
    memcpy(s->clip_keys,keys,(size_t)s->clip_count*sizeof(modelrt_u32));
    return MODELRT_OK;
}
static void gs_try_companion(modelrt_handle *h, gs_state *s, const char *path)
{
    modelrt_u8 *data;
    size_t n, len, save;
    char companion[MODELRT_PATH_MAX];
    modelrt_u32 tracks, keys[GS_MAX_CLIPS],guess;
    int rc;
    len=strlen(path);
    if (len+1u>sizeof(companion) || len<4u) return;
    modelrt_copy_string(companion,sizeof(companion),path);
    companion[len-3u]='N'; companion[len-2u]='A'; companion[len-1u]='C';
    save=h->workspace_used;
    rc=modelrt_read_file_arena(h,companion,&data,&n);
    if (rc!=MODELRT_OK) {
        h->workspace_used=save;
        companion[len-3u]='n'; companion[len-2u]='a'; companion[len-1u]='c';
        rc=modelrt_read_file_arena(h,companion,&data,&n);
    }
    if (rc!=MODELRT_OK) {h->workspace_used=save;return;}
    /* TC declares ntracks; for no-TC try the v19 candidates 1..256. */
    tracks=0u;
    if (n>=16u) {
        modelrt_u32 nr=gs_u32(data);
        size_t off=4u+(size_t)nr*112u;
        if(nr && nr<=500000u && gs_range(off,12u,n)) {
            modelrt_u32 first=gs_u32(data+off+4u);
            if ((first&65535u)==0x4354u) tracks=(first>>24)&255u;
        }
    }
    if (tracks) {
        (void)gs_nac_parse(s,data,n,tracks,keys);
    } else {
        for (guess=1u;guess<=GS_MAX_TRACKS && !s->clip_count;++guess)
            (void)gs_nac_parse(s,data,n,guess,keys);
    }
    /* Discard transient NAC bytes, then store only validated counts. */
    h->workspace_used=save;
    if (s->clip_count) {
        s->clip_keys=(modelrt_u32*)modelrt_arena_alloc(h,(size_t)s->clip_count*sizeof(modelrt_u32),8u);
        if (s->clip_keys) memcpy(s->clip_keys,keys,(size_t)s->clip_count*sizeof(modelrt_u32));
        else s->clip_count=0u;
    }
}
/* TIM is an external sidecar; the runtime never embeds game data. */
static int gs_tim_read(modelrt_handle *h, const char *path,
                       modelrt_u8 **out, size_t *n, size_t save)
{
    int rc;
    h->workspace_used=save;
    rc=modelrt_read_file_arena(h,path,out,n);
    if(rc!=MODELRT_OK)h->workspace_used=save;
    return rc;
}
static void gs_lower_basename(char *p)
{
    char *b=p, *v;
    for(v=p;*v;++v)if(*v=='/'||*v=='\\')b=v+1;
    for(v=b;*v;++v)if(*v>='A'&&*v<='Z')*v=(char)(*v-'A'+'a');
}
/* TIM is an external sidecar; file name casing varies on PSX/PC exports. */
static int gs_load_tim(modelrt_handle *h, gs_state *s, const char *path)
{
    char companion[MODELRT_PATH_MAX];
    modelrt_u8 *tim;
    size_t len=strlen(path), n,save=h->workspace_used;
    int rc;
    if (len+1u>sizeof(companion)||len<4u) return 0;
    modelrt_copy_string(companion,sizeof(companion),path);
    companion[len-3u]='T';companion[len-2u]='I';companion[len-1u]='M';
    rc=gs_tim_read(h,companion,&tim,&n,save);
    if(rc!=MODELRT_OK){
        companion[len-3u]='t';companion[len-2u]='i';companion[len-1u]='m';
        rc=gs_tim_read(h,companion,&tim,&n,save);
    }
    if(rc!=MODELRT_OK){
        gs_lower_basename(companion);
        rc=gs_tim_read(h,companion,&tim,&n,save);
    }
    if(rc!=MODELRT_OK && len>=5u && (path[len-5u]=='B'||path[len-5u]=='b')){
        /* TAIRANTB.HMD uses the same TAIRANT.tim VRAM texture. */
        memmove(companion+len-5u,companion+len-4u,5u);
        len--;
        companion[len-3u]='t';companion[len-2u]='i';companion[len-1u]='m';
        rc=gs_tim_read(h,companion,&tim,&n,save);
    }
    if(rc!=MODELRT_OK)return 0;
    if(n<20u||gs_u32(tim)!=16u||gs_u32(tim+4u)!=2u)return 0;
    s->textures[0].w=gs_u16(tim+16u);s->textures[0].h=gs_u16(tim+18u);
    s->tim_x=gs_u16(tim+12u);s->tim_y=gs_u16(tim+14u);
    if (!s->textures[0].w||!s->textures[0].h||
        s->textures[0].w>2048u||s->textures[0].h>2048u||
        gs_u32(tim+8u)!=12u+s->textures[0].w*s->textures[0].h*2u ||
        n!=20u+(size_t)s->textures[0].w*s->textures[0].h*2u) return 0;
    s->tim=tim;s->tim_size=n;s->textures[0].pixels=20u;s->ntex=1u;
    return 1;
}

static int gs_hmd_open(modelrt_handle *h, gs_state *s, const char *path)
{
    const modelrt_u8 *d=s->bytes;
    size_t n=s->length;
    modelrt_u32 ph,prim,poly,vert,norm,nt,i,code,info,ind,corner,k,tpage,pi,ni,tpx,tpy;
    modelrt_i32 u,v;
    if (n<112u||gs_u32(d)!=0x50u||gs_u32(d+12u)!=3u)return MODELRT_ERR_FORMAT;
    ph=gs_u32(d+8u)*4u;prim=gs_u32(d+20u)*4u;
    if(!gs_range(ph,40u,n)||!gs_range(prim,12u,n))return MODELRT_ERR_FORMAT;
    poly=(gs_u32(d+ph+20u)&0x7fffffffu)*4u;
    vert=(gs_u32(d+ph+24u)&0x7fffffffu)*4u;
    norm=(gs_u32(d+ph+28u)&0x7fffffffu)*4u;
    if(!(vert>0u && norm>vert && poly>norm && poly<n) || (norm-vert)%8u)
        return MODELRT_ERR_FORMAT;
    s->h_verts=(norm-vert)/8u;s->h_norms=(poly-norm)/8u;
    if(s->h_verts>8192u||s->h_norms>8192u)return MODELRT_ERR_FORMAT;
    s->h_vert_off=vert;s->h_norm_off=norm;s->h_poly_off=poly;
    nt=gs_u32(d+prim+8u)&65535u;
    if(!nt||!gs_range(prim+12u,(size_t)nt*12u,n))return MODELRT_ERR_FORMAT;
    if(!gs_load_tim(h,s,path))return MODELRT_ERR_UNSUPPORTED;
    s->hpolys=(gs_hpoly*)modelrt_arena_alloc(h,(size_t)nt*sizeof(gs_hpoly),8u);
    if(!s->hpolys)return MODELRT_ERR_NO_SPACE;
    s->h_polygons=nt;s->h_tris=0u;
    for(i=0u;i<nt;++i){
        size_t b=prim+12u+(size_t)i*12u;
        modelrt_u32 type=gs_u32(d+b);
        info=gs_u32(d+b+4u);ind=gs_u32(d+b+8u);code=type&255u;
        if((info&0x7fffffffu)!=0x10002u || (code!=0x0du&&code!=0x15u))
            return MODELRT_ERR_FORMAT;
        corner=code==0x0du?3u:4u;
        if((size_t)ind>(n-poly)/4u)return MODELRT_ERR_FORMAT;
        s->hpolys[i].offset=poly+ind*4u;s->hpolys[i].corners=corner;
        if(!gs_range(s->hpolys[i].offset,corner==3u?24u:28u,n))return MODELRT_ERR_FORMAT;
        tpage=gs_u16(d+s->hpolys[i].offset+6u);
        tpx=(tpage&15u)*64u;tpy=(tpage&16u)?256u:0u;
        for(k=0u;k<corner;++k){
            size_t p=s->hpolys[i].offset;
            pi=gs_u16(d+p+14u+k*4u);
            ni=gs_u16(d+p+12u+k*4u);
            if(corner==4u && k==0u)ni=gs_u16(d+p+10u);
            u=(modelrt_i32)d[p+k*4u]+(modelrt_i32)tpx-(modelrt_i32)s->tim_x;
            v=(modelrt_i32)d[p+k*4u+1u]+(modelrt_i32)tpy-(modelrt_i32)s->tim_y;
            if(pi>=s->h_verts||ni>=s->h_norms||u<0||v<0||
               (modelrt_u32)u>=s->textures[0].w||(modelrt_u32)v>=s->textures[0].h)
                return MODELRT_ERR_FORMAT;
        }
        s->h_tris+=corner==3u?1u:2u;
    }
    s->nv=s->h_tris*3u;s->nt=s->h_tris;s->np=1u;
    return MODELRT_OK;
}
static int gs_open(modelrt_runtime *rt, modelrt_handle *h, const char *path)
{
    gs_state *s;
    modelrt_u8 *data;
    size_t len;
    int rc;
    s=(gs_state*)modelrt_arena_alloc(h,sizeof(*s),8u);
    if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));h->native_state=s;
    rc=modelrt_read_file_arena(h,path,&data,&len);
    if(rc!=MODELRT_OK)return rc;
    s->bytes=data;s->length=len;
    s->kind=modelrt_ext_eq(path,".nab")?GS_NAB:
            (modelrt_ext_eq(path,".nac")?GS_NAC:GS_HMD);
    if(len>(s->kind==GS_HMD?GS_MAX_HMD_SIZE:GS_MAX_NAB_SIZE))return MODELRT_ERR_LIMIT;
    if (len<8u)return MODELRT_ERR_FORMAT;
    rc=s->kind==GS_NAB?gs_nab_open(h,s):
       (s->kind==GS_NAC?gs_nac_open(h,s):gs_hmd_open(h,s,path));
    if(rc!=MODELRT_OK){modelrt_set_error(rt,"GunSurvivor",rc,"invalid NAB/NAC/HMD layout or missing HMD TIM");return rc;}
    if(s->kind==GS_NAB)gs_try_companion(h,s,path);
    gs_name(h->info.name,sizeof(h->info.name),path);
    h->info.mesh_count=s->np;
    h->info.material_count=s->ntex;
    h->info.texture_count=s->ntex;
    h->info.rigid_part_count=s->kind==GS_NAB?s->np:0u;
    h->info.bone_count=s->kind==GS_NAB?s->nc+1u:0u;
    h->info.animation_count=s->clip_count;
    h->info.capabilities=s->kind==GS_NAC?MODELRT_CAP_ANIMATION:
        (MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS|
        MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS|MODELRT_CAP_TEXTURES|
        MODELRT_CAP_EMBEDDED_TEX);
    if(s->kind==GS_NAB)h->info.capabilities|=MODELRT_CAP_RIGID_HIERARCHY|MODELRT_CAP_SKELETON;
    if(s->clip_count)h->info.capabilities|=MODELRT_CAP_ANIMATION;
    return MODELRT_OK;
}
static void gs_close(modelrt_handle *h) {(void)h;}
static int gs_tri_vertex(gs_state *s, modelrt_u32 vertex, modelrt_u32 joint,
                          modelrt_vertex_view *out)
{
    const modelrt_u8 *d=s->bytes;
    size_t off=s->vertices_offset+(size_t)vertex*36u;
    modelrt_i32 p[3],q;
    modelrt_u32 i, word;
    modelrt_zero(out,sizeof(*out));
    if(!gs_range(off,36u,s->length))return MODELRT_ERR_FORMAT;
    for(i=0u;i<3u;++i){
        word=gs_u32(d+off+(size_t)i*4u);
        if(!gs_float_q16(word,&q))return MODELRT_ERR_FORMAT;
        p[i]=q/256;
    }
    if(!gs_world(s,p,joint,0))return MODELRT_ERR_CODEC;
    out->position.x=gs_meter(p[0]);out->position.y=gs_meter(p[1]);out->position.z=gs_meter(p[2]);
    for(i=0u;i<3u;++i){
        word=gs_u32(d+off+(size_t)(i+3u)*4u);
        if(!gs_float_q16(word,&q))return MODELRT_ERR_FORMAT;
        p[i]=q/256;
    }
    if(!gs_world(s,p,joint,1))return MODELRT_ERR_CODEC;
    out->normal.x=gs_qnorm(p[0]);out->normal.y=gs_qnorm(p[1]);out->normal.z=gs_qnorm(p[2]);
    if(!gs_float_q16(gs_u32(d+off+28u),&out->uv0.x)||
       !gs_float_q16(gs_u32(d+off+32u),&q))return MODELRT_ERR_FORMAT;
    out->uv0.y=65536-q;
    out->rgba[0]=out->rgba[1]=out->rgba[2]=out->rgba[3]=255u;
    out->flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;
    return MODELRT_OK;
}
static int gs_hmd_vertex(gs_state *s, size_t p, modelrt_u32 corners, modelrt_u32 corner,
                          modelrt_vertex_view *out)
{
    const modelrt_u8 *d=s->bytes;
    modelrt_u32 k,pi,ni,tpage,tpx,tpy;
    modelrt_i32 u,v;
    size_t v_off,n_off;
    modelrt_zero(out,sizeof(*out));
    k=corner;
    pi=gs_u16(d+p+14u+k*4u);
    ni=(k==0u && corners==4u) ? gs_u16(d+p+10u) : gs_u16(d+p+12u+k*4u);
    /* For quads, first normal index lives at +10, not +12. */
    v_off=s->h_vert_off+(size_t)pi*8u;
    n_off=s->h_norm_off+(size_t)ni*8u;
    if(!gs_range(v_off,8u,s->length)||!gs_range(n_off,8u,s->length))return MODELRT_ERR_FORMAT;
    out->position.x=(modelrt_q16)(gs_s16(gs_u16(d+v_off+4u))*8192/125);
    out->position.y=(modelrt_q16)(-gs_s16(gs_u16(d+v_off+2u))*8192/125);
    out->position.z=(modelrt_q16)(-gs_s16(gs_u16(d+v_off))*8192/125);
    out->normal.x=(modelrt_q16)(gs_s16(gs_u16(d+n_off+4u))*16);
    out->normal.y=(modelrt_q16)(-gs_s16(gs_u16(d+n_off+2u))*16);
    out->normal.z=(modelrt_q16)(-gs_s16(gs_u16(d+n_off))*16);
    tpage=gs_u16(d+p+6u);
    tpx=(tpage&15u)*64u;tpy=(tpage&16u)?256u:0u;
    u=(modelrt_i32)d[p+k*4u]+(modelrt_i32)tpx-(modelrt_i32)s->tim_x;
    v=(modelrt_i32)d[p+k*4u+1u]+(modelrt_i32)tpy-(modelrt_i32)s->tim_y;
    out->uv0.x=(modelrt_q16)(((modelrt_u32)(2*u+1)*32768u)/s->textures[0].w);
    out->uv0.y=65536-(modelrt_q16)(((modelrt_u32)(2*v+1)*32768u)/s->textures[0].h);
    out->rgba[0]=out->rgba[1]=out->rgba[2]=out->rgba[3]=255u;
    out->flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;
    return MODELRT_OK;
}
static int gs_emit_triangle_nab(gs_state *s, modelrt_u32 part, modelrt_u32 triangle,
                               const modelrt_visit_sink *sink,void *user)
{
    modelrt_triangle_view tr;
    modelrt_u32 k, vertex;
    int rc;
    modelrt_zero(&tr,sizeof(tr));
    tr.material_index=(s->parts[part].texture==0xffffffffu)?-1:(modelrt_i32)s->parts[part].texture;
    tr.mesh_index=part;tr.flags=MODELRT_TRI_DOUBLE_SIDED;
    for(k=0u;k<3u;++k) {
        vertex=triangle*3u+(k==1u?2u:(k==2u?1u:0u));
        rc=gs_tri_vertex(s,vertex,s->parts[part].joint,&tr.v[k]);
        if(rc!=MODELRT_OK)return rc;
    }
    if (sink->on_triangle && sink->on_triangle(user,&tr)!=0)return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int gs_emit_triangle_hmd(gs_state *s, modelrt_u32 poly, modelrt_u32 tri,
                               const modelrt_visit_sink *sink,void *user)
{
    const modelrt_u32 order[6]={0u,2u,1u,0u,3u,2u};
    modelrt_triangle_view tr;
    modelrt_u32 k,corner;
    int rc;
    size_t p=s->hpolys[poly].offset;
    modelrt_zero(&tr,sizeof(tr));tr.material_index=0;tr.mesh_index=0;
    tr.flags=MODELRT_TRI_DOUBLE_SIDED;
    for(k=0u;k<3u;++k){
        corner=order[tri*3u+k];
        rc=gs_hmd_vertex(s,p,s->hpolys[poly].corners,corner,&tr.v[k]);
        if(rc!=MODELRT_OK)return rc;
    }
    if(sink->on_triangle&&sink->on_triangle(user,&tr)!=0)return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int gs_visit(modelrt_handle *h,int animation_index,modelrt_q16 time,
                    const modelrt_visit_sink *sink,void *user)
{
    gs_state *s=(gs_state*)h->native_state;
    modelrt_u32 i,j;
    int rc;
    (void)time;
    if(!s||!sink)return MODELRT_ERR_BAD_ARG;
    if(animation_index!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
    if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
    for(i=0u;i<s->ntex;++i){
        modelrt_material_view mat;
        modelrt_zero(&mat,sizeof(mat));mat.index=i;
        sprintf(mat.name,"GS_%s_tex%u",s->kind==GS_NAB?"NAB":"HMD",(unsigned)i);
        mat.texture_index=(modelrt_i32)i;
        mat.rgba[0]=mat.rgba[1]=mat.rgba[2]=mat.rgba[3]=255u;
        if(sink->on_material&&sink->on_material(user,&mat))return MODELRT_ERR_CALLBACK;
    }
    /* Preserve coordinate hierarchy separately from draw-group identity. */
    if(s->kind==GS_NAB&&sink->on_bone)for(i=0u;i<=s->nc;++i){
        modelrt_bone_view bv;
        modelrt_i32 p[3]={0,0,0};
        modelrt_zero(&bv,sizeof(bv));bv.index=i;
        bv.parent=(i==0u||s->coords[i].parent==i)?-1:(modelrt_i32)s->coords[i].parent;
        sprintf(bv.name,"GS_coord_%u",(unsigned)i);
        if(!gs_world(s,p,i,0))return MODELRT_ERR_CODEC;
        bv.position.x=gs_meter(p[0]);bv.position.y=gs_meter(p[1]);bv.position.z=gs_meter(p[2]);
        if(sink->on_bone(user,&bv))return MODELRT_ERR_CALLBACK;
    }
    if(s->kind==GS_NAB&&sink->on_rigid_part)for(i=0u;i<s->np;++i){
        modelrt_rigid_part_view pv;
        modelrt_i32 p[3]={0,0,0};
        modelrt_zero(&pv,sizeof(pv));pv.index=i;pv.mesh_index=(modelrt_i32)i;
        pv.parent=-1; /* Source parent is a coordinate index, not a mesh index. */
        sprintf(pv.name,"GS_part_%u_joint_%u",(unsigned)i,(unsigned)s->parts[i].joint);
        if(!gs_world(s,p,s->parts[i].joint,0))return MODELRT_ERR_CODEC;
        pv.position.x=gs_meter(p[0]);pv.position.y=gs_meter(p[1]);pv.position.z=gs_meter(p[2]);
        if(sink->on_rigid_part(user,&pv))return MODELRT_ERR_CALLBACK;
    }
    for(i=0u;i<s->np;++i){
        modelrt_mesh_info mesh;
        modelrt_zero(&mesh,sizeof(mesh));mesh.index=i;
        if(s->kind==GS_NAB){
            sprintf(mesh.name,"GS_NAB_part_%u_joint_%u",(unsigned)i,(unsigned)s->parts[i].joint);
            mesh.triangle_count=s->parts[i].count;
        }else {
            modelrt_copy_string(mesh.name,sizeof(mesh.name),"GS_HMD_mesh");
            mesh.triangle_count=s->h_tris;
        }
        mesh.vertex_count=mesh.triangle_count*3u;mesh.material_count=s->ntex;
        if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mesh))return MODELRT_ERR_CALLBACK;
        if(s->kind==GS_NAB){
            for(j=0u;j<s->parts[i].count;++j){
                rc=gs_emit_triangle_nab(s,i,s->parts[i].first+j,sink,user);
                if(rc!=MODELRT_OK)return rc;
            }
        }else{
            for(j=0u;j<s->h_polygons;++j){
                rc=gs_emit_triangle_hmd(s,j,0u,sink,user);
                if(rc!=MODELRT_OK)return rc;
                if(s->hpolys[j].corners==4u){
                    rc=gs_emit_triangle_hmd(s,j,1u,sink,user);
                    if(rc!=MODELRT_OK)return rc;
                }
            }
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(user,&mesh))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int gs_animation_count(const modelrt_handle *h)
{
    gs_state *s=(gs_state*)h->native_state;return s?(int)s->clip_count:0;
}
static int gs_animation_info(modelrt_handle *h,int ix,modelrt_animation_info *out)
{
    gs_state *s=(gs_state*)h->native_state;
    if(!s||!out||ix<0||(modelrt_u32)ix>=s->clip_count)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)ix;
    sprintf(out->name,"GunSurvivor NAC clip %u [unbound]",(unsigned)(ix+1));
    out->source_key_count=s->clip_keys[ix];
    out->flags=MODELRT_ANIM_UNBOUND;
    /* No source-defined duration, frame count or FPS, do not invent one. */
    return MODELRT_OK;
}
static int gs_texture_count(const modelrt_handle *h)
{gs_state *s=(gs_state*)h->native_state;return s?(int)s->ntex:0;}
static int gs_texture_info(modelrt_handle *h,int ix,modelrt_texture_info *out)
{
    gs_state *s=(gs_state*)h->native_state;
    if(!s||!out||ix<0||(modelrt_u32)ix>=s->ntex)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)ix;
    out->kind=MODELRT_TEXTURE_EMBEDDED;
    out->width=s->textures[ix].w;out->height=s->textures[ix].h;
    out->rgba_bytes=out->width*out->height*4u;
    sprintf(out->name,"GunSurvivor_%s_RGB555_%u",s->kind==GS_NAB?"NAB":"TIM",(unsigned)ix);
    return MODELRT_OK;
}
static int gs_texture_decode(modelrt_handle *h,int ix,modelrt_u8 *dst,size_t cap,
                            modelrt_u32 *w,modelrt_u32 *ht,size_t *bytes)
{
    gs_state *s=(gs_state*)h->native_state;
    modelrt_u32 i,n,raw;
    const modelrt_u8 *p;
    if(!s||ix<0||(modelrt_u32)ix>=s->ntex||!w||!ht||!bytes)return MODELRT_ERR_BAD_ARG;
    *w=s->textures[ix].w;*ht=s->textures[ix].h;
    n=(*w)*(*ht);
    *bytes=(size_t)n*4u;
    if(!dst)return MODELRT_OK;
    if(cap<*bytes)return MODELRT_ERR_NO_SPACE;
    p=(s->kind==GS_NAB?s->bytes:s->tim)+s->textures[ix].pixels;
    for(i=0u;i<n;++i){
        raw=gs_u16(p+(size_t)i*2u);
        dst[(size_t)i*4u]=(modelrt_u8)((((raw>>10)&31u)*255u+15u)/31u);
        dst[(size_t)i*4u+1u]=(modelrt_u8)((((raw>>5)&31u)*255u+15u)/31u);
        dst[(size_t)i*4u+2u]=(modelrt_u8)(((raw&31u)*255u+15u)/31u);
        dst[(size_t)i*4u+3u]=s->kind==GS_NAB?(raw&32768u?255u:0u):255u;
    }
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_gunsurvivor89={
    MODELRT_CODEC_GUNSURVIVOR,"GunSurvivor C89 v19 NAB/NAC/HMD",
    gs_probe,gs_open,gs_close,gs_visit,
    gs_animation_count,gs_animation_info,gs_texture_count,gs_texture_info,
    gs_texture_decode,0,0,0,0
};
