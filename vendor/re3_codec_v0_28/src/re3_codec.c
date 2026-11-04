#include "re3_codec.h"
#include <string.h>
#define RE3C_MD2_OBJECT_HEADER_SIZE 24u
#define RE3C_EDD_HEADER_SIZE 8u
static re3c_u16 rd16(const re3c_u8 *p){return (re3c_u16)((re3c_u16)p[0]|((re3c_u16)p[1]<<8));}
static re3c_i16 rds16(const re3c_u8 *p){return (re3c_i16)rd16(p);}
static re3c_u32 rd32(const re3c_u8 *p){return (re3c_u32)p[0]|((re3c_u32)p[1]<<8)|((re3c_u32)p[2]<<16)|((re3c_u32)p[3]<<24);}
static int src_read(const re3c_source *s,re3c_u32 off,void *dst,size_t n){size_t end;long p;if(!s||!dst)return RE3C_ERR_BAD_ARGUMENT;end=(size_t)off+n;if(end<(size_t)off||end>s->size)return RE3C_ERR_RANGE;if(s->memory){memcpy(dst,s->memory+off,n);return RE3C_OK;}if(!s->fp)return RE3C_ERR_NOT_OPEN;p=(long)off;if((re3c_u32)p!=off)return RE3C_ERR_RANGE;if(fseek(s->fp,p,SEEK_SET)!=0)return RE3C_ERR_IO;if(n&&fread(dst,1u,n,s->fp)!=n)return RE3C_ERR_IO;return RE3C_OK;}
static int src_u16(const re3c_source*s,re3c_u32 o,re3c_u16*v){re3c_u8 b[2];int r=src_read(s,o,b,2u);if(r!=RE3C_OK)return r;*v=rd16(b);return RE3C_OK;}
static int src_i16(const re3c_source*s,re3c_u32 o,re3c_i16*v){re3c_u16 x;int r=src_u16(s,o,&x);*v=(re3c_i16)x;return r;}
static int src_u32(const re3c_source*s,re3c_u32 o,re3c_u32*v){re3c_u8 b[4];int r=src_read(s,o,b,4u);if(r!=RE3C_OK)return r;*v=rd32(b);return RE3C_OK;}
static int insec(const re3c_section*s,re3c_u32 r,re3c_u32 n){re3c_u32 e;if(!s||!s->present)return 0;e=r+n;if(e<r)return 0;return e<=s->size;}
static re3c_u32 align4(re3c_u32 x){if(x>0xFFFFFFFCu)return 0u;return (x+3u)&~3u;}
static re3c_vec3fx vadd(re3c_vec3fx a,re3c_vec3fx b){re3c_vec3fx v;v.x=a.x+b.x;v.y=a.y+b.y;v.z=a.z+b.z;return v;}
static re3c_vec3fx posfx(re3c_i16 x,re3c_i16 y,re3c_i16 z){re3c_vec3fx v;v.x=(re3c_i32)x*RE3C_FX_ONE;v.y=(re3c_i32)y*RE3C_FX_ONE;v.z=(re3c_i32)z*RE3C_FX_ONE;return v;}
static void bank_clear(re3c_anim_bank*b,re3c_u32 id){if(!b)return;memset(b,0,sizeof(*b));b->bank_id=(re3c_u8)id;b->hierarchy_source=(re3c_u8)id;b->paired_timeline_bank=0xFFu;}
static void mesh_clear(re3c_mesh*m){if(m)memset(m,0,sizeof(*m));}
static void bind_clear(re3c_bind_bank*b){re3c_u32 i,j;if(!b)return;memset(b,0,sizeof(*b));for(i=0;i<RE3C_MAX_BIND_LIMBS;i++){b->limbs[i].mapped_part=-1;for(j=0;j<256u;j++){b->limbs[i].joint_a[j]=-1;b->limbs[i].joint_b[j]=-1;}}}
int re3c_check_host(void){return sizeof(re3c_u8)==1u&&sizeof(re3c_u16)==2u&&sizeof(re3c_u32)==4u&&sizeof(re3c_i32)==4u;}
const char*re3c_result_string(int c){switch(c){case RE3C_OK:return"ok";case RE3C_ERR_BAD_ARGUMENT:return"bad argument";case RE3C_ERR_IO:return"i/o error";case RE3C_ERR_BAD_FORMAT:return"bad RE3 format";case RE3C_ERR_OVERFLOW:return"static capacity exceeded";case RE3C_ERR_NOT_OPEN:return"model not open";case RE3C_ERR_NOT_PRESENT:return"section/bank not present";case RE3C_ERR_RANGE:return"out of range";default:return"unknown error";}}
const char*re3c_bank_name(re3c_u32 b){switch(b){case 0:return"Bank A / Normal";case 1:return"Bank B / NormalEx1";case 2:return"Damage";case 3:return"Weapon/Aux A";case 4:return"Weapon/Aux B";default:return"Unknown";}}
const char*re3c_section_name(re3c_u32 s){static const char*n[15]={"binary00","binary01","edd_a","emr_a","edd_b","emr_b","edd_damage","emr_damage","binary_weapon","edd_weapon_a","emr_weapon_a","edd_weapon_b","emr_weapon_b","md2_aux","md2_body"};return s<15u?n[s]:"unknown";}
re3c_fx re3c_fx_from_int(re3c_i32 v){return v*RE3C_FX_ONE;} re3c_i32 re3c_fx_to_int(re3c_fx v){return v/RE3C_FX_ONE;}
re3c_fx re3c_fx_mul(re3c_fx a,re3c_fx b){re3c_i32 ah=a/RE3C_FX_ONE,al=a%RE3C_FX_ONE,bh=b/RE3C_FX_ONE,bl=b%RE3C_FX_ONE;return (ah*bh)*RE3C_FX_ONE+ah*bl+bh*al+(al*bl)/RE3C_FX_ONE;}
re3c_fx re3c_fx_div(re3c_fx a,re3c_fx b){re3c_i32 w,r;if(!b)return 0;w=a/b;r=a%b;return w*RE3C_FX_ONE+(r*RE3C_FX_ONE)/b;}
void re3c_model_init(re3c_model*m){re3c_u32 i;if(!m)return;memset(m,0,sizeof(*m));for(i=0;i<RE3C_BANK_COUNT;i++)bank_clear(&m->banks[i],i);bind_clear(&m->bind_a);bind_clear(&m->bind_b);}
void re3c_model_close(re3c_model*m){FILE*fp;if(!m)return;fp=m->source.fp;if(m->source.owns_file&&fp)fclose(fp);re3c_model_init(m);}
static int parse_dir(re3c_model*m){re3c_u8 h[8];re3c_u32 raw[15],i,j,next;int r;r=src_read(&m->source,0u,h,8u);if(r!=RE3C_OK)return r;m->index_offset=rd32(h);m->section_count=(re3c_u16)rd32(h+4);if(m->section_count!=15u)return RE3C_ERR_BAD_FORMAT;if((size_t)m->index_offset+60u>m->source.size)return RE3C_ERR_BAD_FORMAT;r=src_read(&m->source,m->index_offset,raw,60u);if(r!=RE3C_OK)return r;for(i=0;i<15u;i++){raw[i]=rd32((re3c_u8*)raw+i*4u);}/* host-independent reread */
 for(i=0;i<15u;i++){re3c_u8 b[4];r=src_read(&m->source,m->index_offset+i*4u,b,4u);if(r!=RE3C_OK)return r;raw[i]=rd32(b);if(raw[i]&&raw[i]>=m->index_offset)return RE3C_ERR_BAD_FORMAT;}
 for(i=0;i<15u;i++){if(!raw[i]){memset(&m->sections[i],0,sizeof(m->sections[i]));continue;}next=m->index_offset;for(j=0;j<15u;j++)if(raw[j]>raw[i]&&raw[j]<next)next=raw[j];if(next<=raw[i])return RE3C_ERR_BAD_FORMAT;m->sections[i].offset=raw[i];m->sections[i].size=next-raw[i];m->sections[i].present=1u;}return RE3C_OK;}
int re3c_read_section(const re3c_model*m,re3c_u32 s,re3c_u32 o,void*d,size_t n){re3c_u32 n32;const re3c_section*sec;if(!m||!d)return RE3C_ERR_BAD_ARGUMENT;if(!m->is_open)return RE3C_ERR_NOT_OPEN;if(s>=m->section_count||s>=15u)return RE3C_ERR_RANGE;sec=&m->sections[s];if(!sec->present)return RE3C_ERR_NOT_PRESENT;n32=(re3c_u32)n;if((size_t)n32!=n)return RE3C_ERR_OVERFLOW;if(!insec(sec,o,n32))return RE3C_ERR_RANGE;return src_read(&m->source,sec->offset+o,d,n);}
/* TIM */
static int tim_block(const re3c_source*s,re3c_u32 off,re3c_tim_vram*t,re3c_tim_rect*r,re3c_u32*next){re3c_u8 h[12],row[RE3C_TIM_VRAM_W*2u];re3c_u32 len,wc,bc,yy,xx,rb,so;re3c_u16 x,y,w,hh;int rc=src_read(s,off,h,12u);if(rc!=RE3C_OK)return rc;len=rd32(h);x=rd16(h+4);y=rd16(h+6);w=rd16(h+8);hh=rd16(h+10);if(len<12u)return RE3C_ERR_BAD_FORMAT;if((re3c_u32)x+w>RE3C_TIM_VRAM_W||(re3c_u32)y+hh>RE3C_TIM_VRAM_H)return RE3C_ERR_RANGE;wc=(re3c_u32)w*hh;if(wc>0x7FFFFFFFu)return RE3C_ERR_OVERFLOW;bc=wc*2u;if(bc>len-12u||off>0xFFFFFFFFu-len||(size_t)(off+len)>s->size)return RE3C_ERR_BAD_FORMAT;for(yy=0;yy<hh;yy++){rb=(re3c_u32)w*2u;so=off+12u+yy*rb;rc=src_read(s,so,row,(size_t)rb);if(rc!=RE3C_OK)return rc;for(xx=0;xx<w;xx++)t->words[((re3c_u32)y+yy)*RE3C_TIM_VRAM_W+(re3c_u32)x+xx]=rd16(row+xx*2u);}r->x=x;r->y=y;r->width_words=w;r->height=hh;*next=off+len;return RE3C_OK;}
static int tim_source(re3c_tim_vram*t,const re3c_source*s){re3c_u8 h[8];re3c_u32 o=8u;int r;if(!t||!s)return RE3C_ERR_BAD_ARGUMENT;if(s->size<20u)return RE3C_ERR_BAD_FORMAT;memset(t,0,sizeof(*t));r=src_read(s,0u,h,8u);if(r!=RE3C_OK)return r;if(rd32(h)!=0x10u)return RE3C_ERR_BAD_FORMAT;t->flags=rd32(h+4);t->bpp_code=(re3c_u8)(t->flags&7u);t->has_clut=(re3c_u8)((t->flags&8u)?1u:0u);if(t->has_clut){r=tim_block(s,o,t,&t->clut_rect,&o);if(r!=RE3C_OK)return r;}r=tim_block(s,o,t,&t->image_rect,&o);if(r!=RE3C_OK)return r;t->loaded=1u;return RE3C_OK;}
void re3c_tim_init(re3c_tim_vram*t){if(t)memset(t,0,sizeof(*t));}
int re3c_tim_load_file(re3c_tim_vram*t,const char*p){FILE*f;long e;re3c_source s;int r;if(!t||!p)return RE3C_ERR_BAD_ARGUMENT;f=fopen(p,"rb");if(!f)return RE3C_ERR_IO;if(fseek(f,0L,SEEK_END)!=0){fclose(f);return RE3C_ERR_IO;}e=ftell(f);if(e<0L||(long)(re3c_u32)e!=e){fclose(f);return RE3C_ERR_OVERFLOW;}if(fseek(f,0L,SEEK_SET)!=0){fclose(f);return RE3C_ERR_IO;}memset(&s,0,sizeof(s));s.fp=f;s.size=(size_t)e;r=tim_source(t,&s);fclose(f);return r;}
int re3c_tim_load_memory(re3c_tim_vram*t,const void*d,size_t n){re3c_source s;if(!t||!d)return RE3C_ERR_BAD_ARGUMENT;memset(&s,0,sizeof(s));s.memory=(const re3c_u8*)d;s.size=n;return tim_source(t,&s);}
int re3c_tim_sample_raw(const re3c_tim_vram*t,re3c_u16 page,re3c_u16 clut,re3c_u8 u,re3c_u8 v,re3c_u16*out){re3c_u32 tx,ty,d,wx,w,idx,cx,cy;if(!t||!out)return RE3C_ERR_BAD_ARGUMENT;if(!t->loaded)return RE3C_ERR_NOT_OPEN;tx=((re3c_u32)page&15u)*64u;ty=(((re3c_u32)page>>4)&1u)*256u+v;d=((re3c_u32)page>>7)&3u;if(ty>=512u)return RE3C_ERR_RANGE;if(d==0u){wx=tx+(u>>2);if(wx>=1024u)return RE3C_ERR_RANGE;w=t->words[ty*1024u+wx];idx=(w>>(((re3c_u32)u&3u)*4u))&15u;}else if(d==1u){wx=tx+(u>>1);if(wx>=1024u)return RE3C_ERR_RANGE;w=t->words[ty*1024u+wx];idx=(w>>(((re3c_u32)u&1u)*8u))&255u;}else if(d==2u){wx=tx+u;if(wx>=1024u)return RE3C_ERR_RANGE;*out=t->words[ty*1024u+wx];return RE3C_OK;}else return RE3C_ERR_NOT_PRESENT;cx=((re3c_u32)clut&63u)*16u+idx;cy=((re3c_u32)clut>>6)&511u;if(cx>=1024u||cy>=512u)return RE3C_ERR_RANGE;*out=t->words[cy*1024u+cx];return RE3C_OK;}
re3c_u32 re3c_psx_color_to_argb(re3c_u16 c){re3c_u32 r,g,b;if(c==0u)return 0u;r=c&31u;g=(c>>5)&31u;b=(c>>10)&31u;r=(r<<3)|(r>>2);g=(g<<3)|(g>>2);b=(b<<3)|(b>>2);return 0xFF000000u|(r<<16)|(g<<8)|b;}
int re3c_tim_sample_argb(const re3c_tim_vram*t,re3c_u16 p,re3c_u16 c,re3c_u8 u,re3c_u8 v,re3c_u32*out){re3c_u16 raw;int r;if(!out)return RE3C_ERR_BAD_ARGUMENT;r=re3c_tim_sample_raw(t,p,c,u,v,&raw);if(r!=RE3C_OK)return r;*out=re3c_psx_color_to_argb(raw);return RE3C_OK;}
static int compute_world(re3c_anim_bank*b){re3c_u32 done=0u,pass,i;if(!b||!b->joint_count)return RE3C_ERR_NOT_PRESENT;for(pass=0;pass<b->joint_count+1u;pass++){for(i=0;i<b->joint_count;i++){re3c_i16 p=b->joints[i].parent;if(p<0){b->joints[i].world_pos=b->joints[i].local_pos;done|=(1u);continue;}if((re3c_u32)p>=b->joint_count)return RE3C_ERR_BAD_FORMAT;if(p<(re3c_i16)i||pass){b->joints[i].world_pos=vadd(b->joints[(re3c_u32)p].world_pos,b->joints[i].local_pos);}}}for(i=0;i<b->joint_count;i++){if(b->joints[i].parent>=0&&(re3c_u32)b->joints[i].parent>=b->joint_count)return RE3C_ERR_BAD_FORMAT;}return done?RE3C_OK:RE3C_ERR_BAD_FORMAT;}
static int copy_hierarchy(re3c_anim_bank*d,const re3c_anim_bank*s){re3c_u32 i;if(!d||!s||!s->present||d->joint_count!=s->joint_count)return RE3C_ERR_BAD_FORMAT;d->child_ref_count=s->child_ref_count;if(d->child_ref_count>RE3C_MAX_CHILD_REFS)return RE3C_ERR_OVERFLOW;for(i=0;i<d->joint_count;i++)d->joints[i]=s->joints[i];memcpy(d->child_refs,s->child_refs,d->child_ref_count);d->hierarchy_source=s->bank_id;d->hierarchy_inherited=1u;return RE3C_OK;}
static int parse_edd(re3c_model*m,const re3c_section*e,re3c_anim_bank*b){re3c_u8 h[8],k[2];re3c_u16 first;re3c_u32 ci,cursor=0u;int r;if(!e->present)return RE3C_ERR_NOT_PRESENT;if(e->size<4u)return RE3C_ERR_BAD_FORMAT;r=src_read(&m->source,e->offset,h,4u);if(r!=RE3C_OK)return r;first=rd16(h+2);if(first%RE3C_EDD_HEADER_SIZE)return RE3C_ERR_BAD_FORMAT;b->clip_count=(re3c_u16)(first/RE3C_EDD_HEADER_SIZE);if(!b->clip_count||b->clip_count>RE3C_MAX_CLIPS_PER_BANK)return RE3C_ERR_OVERFLOW;if(!insec(e,0u,(re3c_u32)b->clip_count*8u))return RE3C_ERR_BAD_FORMAT;for(ci=0;ci<b->clip_count;ci++){re3c_u16 n,off,counter;r=src_read(&m->source,e->offset+ci*8u,h,8u);if(r!=RE3C_OK)return r;n=rd16(h);off=rd16(h+2);counter=rd16(h+4);if(cursor+n>RE3C_MAX_KEYS_PER_BANK||!insec(e,off,(re3c_u32)n*2u))return RE3C_ERR_OVERFLOW;b->clips[ci].counter=counter;b->clips[ci].key_count=n;b->clips[ci].first_key=cursor;while(n--){re3c_key*q=&b->keys[cursor++];r=src_read(&m->source,e->offset+off,k,2u);if(r!=RE3C_OK)return r;off=(re3c_u16)(off+2u);memset(q,0,sizeof(*q));q->low=k[0];q->flags=k[1];q->key_kind=(re3c_u8)((q->flags&1u)?RE3C_KEY_INTERPOLATED:RE3C_KEY_EXPLICIT);q->paired_bank=0xFFu;q->frame_index=q->key_kind==RE3C_KEY_EXPLICIT?(re3c_u32)counter+q->low:RE3C_FRAME_NONE;}}b->key_count=cursor;return RE3C_OK;}
static int parse_emr(re3c_model*m,const re3c_section*e,re3c_anim_bank*b,const re3c_anim_bank*inherit){re3c_u8 h[8],v[6],a[4];re3c_u16 pjoint,pframes,nj,fs;re3c_u32 i,j;int r;if(!e->present)return RE3C_ERR_NOT_PRESENT;if(e->size<8u)return RE3C_ERR_BAD_FORMAT;r=src_read(&m->source,e->offset,h,8u);if(r!=RE3C_OK)return r;pjoint=rd16(h);pframes=rd16(h+2);nj=rd16(h+4);fs=rd16(h+6);if(!nj||nj>RE3C_MAX_PARTS||fs<8u)return RE3C_ERR_BAD_FORMAT;b->joint_count=nj;b->frame_offset=pframes;b->frame_size=fs;b->emr_size=e->size;b->physical_frame_count=(pframes<=e->size)?(e->size-pframes)/fs:0u;if(pframes==8u){if(inherit&&inherit->joint_count==nj)return copy_hierarchy(b,inherit);b->hierarchy_source=0xFFu;b->hierarchy_inherited=1u;return RE3C_OK;}if(!insec(e,8u,(re3c_u32)nj*6u)||!insec(e,pjoint,(re3c_u32)nj*4u))return RE3C_ERR_BAD_FORMAT;for(i=0;i<nj;i++){r=src_read(&m->source,e->offset+8u+i*6u,v,6u);if(r!=RE3C_OK)return r;b->joints[i].local_pos=posfx(rds16(v),rds16(v+2),rds16(v+4));b->joints[i].parent=-1;b->joints[i].first_child_ref=0u;b->joints[i].child_count=0u;}
 b->child_ref_count=0u;for(i=0;i<nj;i++){re3c_u16 count,ptr;r=src_read(&m->source,e->offset+pjoint+i*4u,a,4u);if(r!=RE3C_OK)return r;count=rd16(a);ptr=rd16(a+2);b->joints[i].first_child_ref=(re3c_u16)b->child_ref_count;b->joints[i].child_count=count;if(b->child_ref_count+count>RE3C_MAX_CHILD_REFS||!insec(e,(re3c_u32)pjoint+ptr,count))return RE3C_ERR_BAD_FORMAT;for(j=0;j<count;j++){re3c_u8 ch;r=src_read(&m->source,e->offset+(re3c_u32)pjoint+ptr+j,&ch,1u);if(r!=RE3C_OK)return r;if(ch>=nj||ch==i||b->joints[ch].parent>=0)return RE3C_ERR_BAD_FORMAT;b->child_refs[b->child_ref_count++]=ch;b->joints[ch].parent=(re3c_i16)i;}}
 b->hierarchy_source=b->bank_id;b->hierarchy_inherited=0u;return compute_world(b);}
int re3c_parse_bank(re3c_model*m,re3c_u32 id){static const re3c_u8 es[5]={2,4,6,9,11},ms[5]={3,5,7,10,12};re3c_anim_bank*b;const re3c_anim_bank*inh=0;int r;if(!m||id>=5u)return RE3C_ERR_BAD_ARGUMENT;if(!m->is_open)return RE3C_ERR_NOT_OPEN;if(!m->sections[es[id]].present||!m->sections[ms[id]].present||m->sections[es[id]].size<=4u)return RE3C_ERR_NOT_PRESENT;b=&m->banks[id];bank_clear(b,id);b->present=1u;b->geometry_target=(re3c_u8)(id==RE3C_BANK_WEAPON_A||id==RE3C_BANK_WEAPON_B?RE3C_GEOMETRY_AUX:RE3C_GEOMETRY_BODY);b->geometry_present=(re3c_u8)((b->geometry_target==RE3C_GEOMETRY_AUX)?m->sections[RE3C_SEC_MD2_AUX].size>4u:1u);r=parse_edd(m,&m->sections[es[id]],b);if(r!=RE3C_OK)return r;if(id==RE3C_BANK_B||id==RE3C_BANK_DAMAGE)inh=&m->banks[RE3C_BANK_A];else if(id==RE3C_BANK_WEAPON_B)inh=&m->banks[RE3C_BANK_WEAPON_A];r=parse_emr(m,&m->sections[ms[id]],b,inh);if(r!=RE3C_OK)return r;return RE3C_OK;}
static void mark_keys(re3c_model*m){re3c_u32 bi,ci,ki;for(bi=0;bi<5u;bi++){re3c_anim_bank*b=&m->banks[bi];if(!b->present)continue;for(ci=0;ci<b->clip_count;ci++){re3c_clip*c=&b->clips[ci];for(ki=0;ki<c->key_count;ki++){re3c_key*k=&b->keys[c->first_key+ki];if(k->key_kind==RE3C_KEY_EXPLICIT){k->resolvable=1u;if(k->frame_index<b->physical_frame_count)k->pose_available=1u;else if(bi==RE3C_BANK_WEAPON_A&&m->banks[RE3C_BANK_WEAPON_B].present){re3c_anim_bank*p=&m->banks[RE3C_BANK_WEAPON_B];if(ci<p->clip_count&&ki<p->clips[ci].key_count){re3c_key*pk=&p->keys[p->clips[ci].first_key+ki];if(pk->key_kind==RE3C_KEY_EXPLICIT&&pk->frame_index<p->physical_frame_count){k->timeline_only=1u;k->paired_bank=RE3C_BANK_WEAPON_B;}}}}else{k->resolvable=1u;k->pose_available=1u;}}}}}
int re3c_parse_all_banks(re3c_model*m){re3c_u32 i;int first=RE3C_OK,r;for(i=0;i<5u;i++){r=re3c_parse_bank(m,i);if(r==RE3C_ERR_NOT_PRESENT)continue;if(r!=RE3C_OK&&first==RE3C_OK)first=r;}mark_keys(m);return first;}
static int addface(re3c_mesh*m,const re3c_face*f){if(m->face_count>=RE3C_MAX_FACES)return RE3C_ERR_OVERFLOW;m->faces[m->face_count++]=*f;return RE3C_OK;}
static int parse_md2(re3c_model*m,const re3c_section*s,re3c_mesh*mesh,re3c_u32 hb){re3c_u8 h[8],oh[24],d[16];re3c_u32 n,oi,vi;int r;if(!s->present||s->size<=4u)return RE3C_ERR_NOT_PRESENT;r=src_read(&m->source,s->offset,h,8u);if(r!=RE3C_OK)return r;n=rd32(h+4);if(!n||n>RE3C_MAX_PARTS||!insec(s,8u,n*24u))return RE3C_ERR_BAD_FORMAT;mesh_clear(mesh);mesh->part_count=(re3c_u16)n;mesh->hierarchy_bank=(re3c_u8)hb;for(oi=0;oi<n;oi++){re3c_u16 vo,no,vc,nc,to,qo,tc,qc;re3c_mesh_part*p=&mesh->parts[oi];r=src_read(&m->source,s->offset+8u+oi*24u,oh,24u);if(r!=RE3C_OK)return r;vo=rd16(oh);no=rd16(oh+4);vc=rd16(oh+8);nc=rd16(oh+10);to=rd16(oh+12);qo=rd16(oh+16);tc=rd16(oh+20);qc=rd16(oh+22);if(!vc||vc>256u)return RE3C_ERR_BAD_FORMAT;if(!nc)nc=vc;if(nc>256u||mesh->vertex_count+vc>RE3C_MAX_VERTICES||mesh->normal_count+nc>RE3C_MAX_NORMALS)return RE3C_ERR_OVERFLOW;if(!insec(s,8u+vo,(re3c_u32)vc*8u)||!insec(s,8u+no,(re3c_u32)nc*8u)||!insec(s,8u+to,(re3c_u32)tc*12u)||!insec(s,8u+qo,(re3c_u32)qc*16u))return RE3C_ERR_BAD_FORMAT;memset(p,0,sizeof(*p));p->first_vertex=mesh->vertex_count;p->vertex_count=vc;p->first_normal=mesh->normal_count;p->normal_count=nc;p->first_face=mesh->face_count;p->native_index=(re3c_u16)oi;p->parent_part=-1;p->driver_joint=-1;p->bind_limb=-1;p->morph_track=-1;if(hb<5u&&oi<m->banks[hb].joint_count){p->local_pos=m->banks[hb].joints[oi].local_pos;p->world_pos=m->banks[hb].joints[oi].world_pos;p->parent_part=m->banks[hb].joints[oi].parent;}
 for(vi=0;vi<vc;vi++){r=src_read(&m->source,s->offset+8u+vo+vi*8u,d,8u);if(r!=RE3C_OK)return r;mesh->vertices[mesh->vertex_count++].pos=posfx(rds16(d),rds16(d+2),rds16(d+4));}for(vi=0;vi<nc;vi++){r=src_read(&m->source,s->offset+8u+no+vi*8u,d,8u);if(r!=RE3C_OK)return r;mesh->normals[mesh->normal_count].n.x=rds16(d);mesh->normals[mesh->normal_count].n.y=rds16(d+2);mesh->normals[mesh->normal_count].n.z=rds16(d+4);mesh->normal_count++;}
 for(vi=0;vi<tc;vi++){re3c_face f;memset(&f,0,sizeof(f));r=src_read(&m->source,s->offset+8u+to+vi*12u,d,12u);if(r!=RE3C_OK)return r;if(d[7]>=vc||d[10]>=vc||d[11]>=vc)return RE3C_ERR_BAD_FORMAT;f.vertex_count=3;f.clut=rd16(d+2);f.page=d[6];f.uv[0].u=d[0];f.uv[0].v=d[1];f.uv[1].u=d[4];f.uv[1].v=d[5];f.uv[2].u=d[8];f.uv[2].v=d[9];f.vertex[0]=p->first_vertex+d[7];f.vertex[1]=p->first_vertex+d[10];f.vertex[2]=p->first_vertex+d[11];r=addface(mesh,&f);if(r!=RE3C_OK)return r;}
 for(vi=0;vi<qc;vi++){re3c_face f;memset(&f,0,sizeof(f));r=src_read(&m->source,s->offset+8u+qo+vi*16u,d,16u);if(r!=RE3C_OK)return r;if(d[10]>=vc||d[11]>=vc||d[14]>=vc||d[15]>=vc)return RE3C_ERR_BAD_FORMAT;f.vertex_count=4;f.clut=rd16(d+2);f.page=rd16(d+6);f.uv[0].u=d[0];f.uv[0].v=d[1];f.uv[1].u=d[4];f.uv[1].v=d[5];f.uv[2].u=d[8];f.uv[2].v=d[9];f.uv[3].u=d[12];f.uv[3].v=d[13];f.vertex[0]=p->first_vertex+d[10];f.vertex[1]=p->first_vertex+d[11];f.vertex[2]=p->first_vertex+d[14];f.vertex[3]=p->first_vertex+d[15];r=addface(mesh,&f);if(r!=RE3C_OK)return r;}p->face_count=mesh->face_count-p->first_face;}return RE3C_OK;}
int re3c_parse_body_mesh(re3c_model*m){if(!m)return RE3C_ERR_BAD_ARGUMENT;return parse_md2(m,&m->sections[14],&m->body,0);}
int re3c_parse_aux_mesh(re3c_model*m){int r;if(!m)return RE3C_ERR_BAD_ARGUMENT;r=parse_md2(m,&m->sections[13],&m->aux,3);if(r==RE3C_OK)m->has_aux_mesh=1u;return r;}
static const re3c_i16 sin_q12[4096] = {
    0, 6, 13, 19, 25, 31, 38, 44, 50, 57, 63, 69, 75, 82, 88, 94,
    101, 107, 113, 119, 126, 132, 138, 144, 151, 157, 163, 170, 176, 182, 188, 195,
    201, 207, 214, 220, 226, 232, 239, 245, 251, 257, 264, 270, 276, 283, 289, 295,
    301, 308, 314, 320, 326, 333, 339, 345, 351, 358, 364, 370, 376, 383, 389, 395,
    401, 408, 414, 420, 426, 433, 439, 445, 451, 458, 464, 470, 476, 483, 489, 495,
    501, 508, 514, 520, 526, 533, 539, 545, 551, 557, 564, 570, 576, 582, 589, 595,
    601, 607, 613, 620, 626, 632, 638, 644, 651, 657, 663, 669, 675, 682, 688, 694,
    700, 706, 713, 719, 725, 731, 737, 744, 750, 756, 762, 768, 774, 781, 787, 793,
    799, 805, 811, 818, 824, 830, 836, 842, 848, 854, 861, 867, 873, 879, 885, 891,
    897, 904, 910, 916, 922, 928, 934, 940, 946, 953, 959, 965, 971, 977, 983, 989,
    995, 1001, 1007, 1014, 1020, 1026, 1032, 1038, 1044, 1050, 1056, 1062, 1068, 1074, 1080, 1086,
    1092, 1099, 1105, 1111, 1117, 1123, 1129, 1135, 1141, 1147, 1153, 1159, 1165, 1171, 1177, 1183,
    1189, 1195, 1201, 1207, 1213, 1219, 1225, 1231, 1237, 1243, 1249, 1255, 1261, 1267, 1273, 1279,
    1285, 1291, 1297, 1303, 1309, 1315, 1321, 1327, 1332, 1338, 1344, 1350, 1356, 1362, 1368, 1374,
    1380, 1386, 1392, 1398, 1404, 1409, 1415, 1421, 1427, 1433, 1439, 1445, 1451, 1457, 1462, 1468,
    1474, 1480, 1486, 1492, 1498, 1503, 1509, 1515, 1521, 1527, 1533, 1538, 1544, 1550, 1556, 1562,
    1567, 1573, 1579, 1585, 1591, 1596, 1602, 1608, 1614, 1620, 1625, 1631, 1637, 1643, 1648, 1654,
    1660, 1666, 1671, 1677, 1683, 1689, 1694, 1700, 1706, 1711, 1717, 1723, 1729, 1734, 1740, 1746,
    1751, 1757, 1763, 1768, 1774, 1780, 1785, 1791, 1797, 1802, 1808, 1813, 1819, 1825, 1830, 1836,
    1842, 1847, 1853, 1858, 1864, 1870, 1875, 1881, 1886, 1892, 1898, 1903, 1909, 1914, 1920, 1925,
    1931, 1936, 1942, 1947, 1953, 1958, 1964, 1970, 1975, 1981, 1986, 1992, 1997, 2002, 2008, 2013,
    2019, 2024, 2030, 2035, 2041, 2046, 2052, 2057, 2062, 2068, 2073, 2079, 2084, 2090, 2095, 2100,
    2106, 2111, 2117, 2122, 2127, 2133, 2138, 2143, 2149, 2154, 2159, 2165, 2170, 2175, 2181, 2186,
    2191, 2197, 2202, 2207, 2213, 2218, 2223, 2228, 2234, 2239, 2244, 2249, 2255, 2260, 2265, 2270,
    2276, 2281, 2286, 2291, 2296, 2302, 2307, 2312, 2317, 2322, 2328, 2333, 2338, 2343, 2348, 2353,
    2359, 2364, 2369, 2374, 2379, 2384, 2389, 2394, 2399, 2405, 2410, 2415, 2420, 2425, 2430, 2435,
    2440, 2445, 2450, 2455, 2460, 2465, 2470, 2475, 2480, 2485, 2490, 2495, 2500, 2505, 2510, 2515,
    2520, 2525, 2530, 2535, 2540, 2545, 2550, 2555, 2559, 2564, 2569, 2574, 2579, 2584, 2589, 2594,
    2598, 2603, 2608, 2613, 2618, 2623, 2628, 2632, 2637, 2642, 2647, 2652, 2656, 2661, 2666, 2671,
    2675, 2680, 2685, 2690, 2694, 2699, 2704, 2709, 2713, 2718, 2723, 2727, 2732, 2737, 2741, 2746,
    2751, 2755, 2760, 2765, 2769, 2774, 2779, 2783, 2788, 2792, 2797, 2802, 2806, 2811, 2815, 2820,
    2824, 2829, 2833, 2838, 2843, 2847, 2852, 2856, 2861, 2865, 2870, 2874, 2878, 2883, 2887, 2892,
    2896, 2901, 2905, 2910, 2914, 2918, 2923, 2927, 2932, 2936, 2940, 2945, 2949, 2953, 2958, 2962,
    2967, 2971, 2975, 2979, 2984, 2988, 2992, 2997, 3001, 3005, 3009, 3014, 3018, 3022, 3026, 3031,
    3035, 3039, 3043, 3048, 3052, 3056, 3060, 3064, 3068, 3073, 3077, 3081, 3085, 3089, 3093, 3097,
    3102, 3106, 3110, 3114, 3118, 3122, 3126, 3130, 3134, 3138, 3142, 3146, 3150, 3154, 3158, 3162,
    3166, 3170, 3174, 3178, 3182, 3186, 3190, 3194, 3198, 3202, 3206, 3210, 3214, 3217, 3221, 3225,
    3229, 3233, 3237, 3241, 3244, 3248, 3252, 3256, 3260, 3264, 3267, 3271, 3275, 3279, 3282, 3286,
    3290, 3294, 3297, 3301, 3305, 3309, 3312, 3316, 3320, 3323, 3327, 3331, 3334, 3338, 3342, 3345,
    3349, 3352, 3356, 3360, 3363, 3367, 3370, 3374, 3378, 3381, 3385, 3388, 3392, 3395, 3399, 3402,
    3406, 3409, 3413, 3416, 3420, 3423, 3426, 3430, 3433, 3437, 3440, 3444, 3447, 3450, 3454, 3457,
    3461, 3464, 3467, 3471, 3474, 3477, 3481, 3484, 3487, 3490, 3494, 3497, 3500, 3504, 3507, 3510,
    3513, 3516, 3520, 3523, 3526, 3529, 3532, 3536, 3539, 3542, 3545, 3548, 3551, 3555, 3558, 3561,
    3564, 3567, 3570, 3573, 3576, 3579, 3582, 3585, 3588, 3591, 3594, 3597, 3600, 3603, 3606, 3609,
    3612, 3615, 3618, 3621, 3624, 3627, 3630, 3633, 3636, 3639, 3642, 3644, 3647, 3650, 3653, 3656,
    3659, 3661, 3664, 3667, 3670, 3673, 3675, 3678, 3681, 3684, 3686, 3689, 3692, 3695, 3697, 3700,
    3703, 3705, 3708, 3711, 3713, 3716, 3719, 3721, 3724, 3727, 3729, 3732, 3734, 3737, 3739, 3742,
    3745, 3747, 3750, 3752, 3755, 3757, 3760, 3762, 3765, 3767, 3770, 3772, 3775, 3777, 3779, 3782,
    3784, 3787, 3789, 3791, 3794, 3796, 3798, 3801, 3803, 3805, 3808, 3810, 3812, 3815, 3817, 3819,
    3822, 3824, 3826, 3828, 3831, 3833, 3835, 3837, 3839, 3842, 3844, 3846, 3848, 3850, 3852, 3854,
    3857, 3859, 3861, 3863, 3865, 3867, 3869, 3871, 3873, 3875, 3877, 3879, 3881, 3883, 3885, 3887,
    3889, 3891, 3893, 3895, 3897, 3899, 3901, 3903, 3905, 3907, 3909, 3910, 3912, 3914, 3916, 3918,
    3920, 3921, 3923, 3925, 3927, 3929, 3930, 3932, 3934, 3936, 3937, 3939, 3941, 3943, 3944, 3946,
    3948, 3949, 3951, 3953, 3954, 3956, 3958, 3959, 3961, 3962, 3964, 3965, 3967, 3969, 3970, 3972,
    3973, 3975, 3976, 3978, 3979, 3981, 3982, 3984, 3985, 3987, 3988, 3989, 3991, 3992, 3994, 3995,
    3996, 3998, 3999, 4001, 4002, 4003, 4005, 4006, 4007, 4008, 4010, 4011, 4012, 4014, 4015, 4016,
    4017, 4019, 4020, 4021, 4022, 4023, 4024, 4026, 4027, 4028, 4029, 4030, 4031, 4032, 4034, 4035,
    4036, 4037, 4038, 4039, 4040, 4041, 4042, 4043, 4044, 4045, 4046, 4047, 4048, 4049, 4050, 4051,
    4052, 4053, 4053, 4054, 4055, 4056, 4057, 4058, 4059, 4060, 4060, 4061, 4062, 4063, 4064, 4064,
    4065, 4066, 4067, 4067, 4068, 4069, 4070, 4070, 4071, 4072, 4072, 4073, 4074, 4074, 4075, 4076,
    4076, 4077, 4077, 4078, 4079, 4079, 4080, 4080, 4081, 4081, 4082, 4082, 4083, 4083, 4084, 4084,
    4085, 4085, 4086, 4086, 4087, 4087, 4088, 4088, 4088, 4089, 4089, 4089, 4090, 4090, 4090, 4091,
    4091, 4091, 4092, 4092, 4092, 4092, 4093, 4093, 4093, 4093, 4094, 4094, 4094, 4094, 4094, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
    4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4094, 4094, 4094, 4094, 4094, 4093, 4093, 4093, 4093, 4092, 4092, 4092, 4092, 4091,
    4091, 4091, 4090, 4090, 4090, 4089, 4089, 4089, 4088, 4088, 4088, 4087, 4087, 4086, 4086, 4085,
    4085, 4084, 4084, 4083, 4083, 4082, 4082, 4081, 4081, 4080, 4080, 4079, 4079, 4078, 4077, 4077,
    4076, 4076, 4075, 4074, 4074, 4073, 4072, 4072, 4071, 4070, 4070, 4069, 4068, 4067, 4067, 4066,
    4065, 4064, 4064, 4063, 4062, 4061, 4060, 4060, 4059, 4058, 4057, 4056, 4055, 4054, 4053, 4053,
    4052, 4051, 4050, 4049, 4048, 4047, 4046, 4045, 4044, 4043, 4042, 4041, 4040, 4039, 4038, 4037,
    4036, 4035, 4034, 4032, 4031, 4030, 4029, 4028, 4027, 4026, 4024, 4023, 4022, 4021, 4020, 4019,
    4017, 4016, 4015, 4014, 4012, 4011, 4010, 4008, 4007, 4006, 4005, 4003, 4002, 4001, 3999, 3998,
    3996, 3995, 3994, 3992, 3991, 3989, 3988, 3987, 3985, 3984, 3982, 3981, 3979, 3978, 3976, 3975,
    3973, 3972, 3970, 3969, 3967, 3965, 3964, 3962, 3961, 3959, 3958, 3956, 3954, 3953, 3951, 3949,
    3948, 3946, 3944, 3943, 3941, 3939, 3937, 3936, 3934, 3932, 3930, 3929, 3927, 3925, 3923, 3921,
    3920, 3918, 3916, 3914, 3912, 3910, 3909, 3907, 3905, 3903, 3901, 3899, 3897, 3895, 3893, 3891,
    3889, 3887, 3885, 3883, 3881, 3879, 3877, 3875, 3873, 3871, 3869, 3867, 3865, 3863, 3861, 3859,
    3857, 3854, 3852, 3850, 3848, 3846, 3844, 3842, 3839, 3837, 3835, 3833, 3831, 3828, 3826, 3824,
    3822, 3819, 3817, 3815, 3812, 3810, 3808, 3805, 3803, 3801, 3798, 3796, 3794, 3791, 3789, 3787,
    3784, 3782, 3779, 3777, 3775, 3772, 3770, 3767, 3765, 3762, 3760, 3757, 3755, 3752, 3750, 3747,
    3745, 3742, 3739, 3737, 3734, 3732, 3729, 3727, 3724, 3721, 3719, 3716, 3713, 3711, 3708, 3705,
    3703, 3700, 3697, 3695, 3692, 3689, 3686, 3684, 3681, 3678, 3675, 3673, 3670, 3667, 3664, 3661,
    3659, 3656, 3653, 3650, 3647, 3644, 3642, 3639, 3636, 3633, 3630, 3627, 3624, 3621, 3618, 3615,
    3612, 3609, 3606, 3603, 3600, 3597, 3594, 3591, 3588, 3585, 3582, 3579, 3576, 3573, 3570, 3567,
    3564, 3561, 3558, 3555, 3551, 3548, 3545, 3542, 3539, 3536, 3532, 3529, 3526, 3523, 3520, 3516,
    3513, 3510, 3507, 3504, 3500, 3497, 3494, 3490, 3487, 3484, 3481, 3477, 3474, 3471, 3467, 3464,
    3461, 3457, 3454, 3450, 3447, 3444, 3440, 3437, 3433, 3430, 3426, 3423, 3420, 3416, 3413, 3409,
    3406, 3402, 3399, 3395, 3392, 3388, 3385, 3381, 3378, 3374, 3370, 3367, 3363, 3360, 3356, 3352,
    3349, 3345, 3342, 3338, 3334, 3331, 3327, 3323, 3320, 3316, 3312, 3309, 3305, 3301, 3297, 3294,
    3290, 3286, 3282, 3279, 3275, 3271, 3267, 3264, 3260, 3256, 3252, 3248, 3244, 3241, 3237, 3233,
    3229, 3225, 3221, 3217, 3214, 3210, 3206, 3202, 3198, 3194, 3190, 3186, 3182, 3178, 3174, 3170,
    3166, 3162, 3158, 3154, 3150, 3146, 3142, 3138, 3134, 3130, 3126, 3122, 3118, 3114, 3110, 3106,
    3102, 3097, 3093, 3089, 3085, 3081, 3077, 3073, 3068, 3064, 3060, 3056, 3052, 3048, 3043, 3039,
    3035, 3031, 3026, 3022, 3018, 3014, 3009, 3005, 3001, 2997, 2992, 2988, 2984, 2979, 2975, 2971,
    2967, 2962, 2958, 2953, 2949, 2945, 2940, 2936, 2932, 2927, 2923, 2918, 2914, 2910, 2905, 2901,
    2896, 2892, 2887, 2883, 2878, 2874, 2870, 2865, 2861, 2856, 2852, 2847, 2843, 2838, 2833, 2829,
    2824, 2820, 2815, 2811, 2806, 2802, 2797, 2792, 2788, 2783, 2779, 2774, 2769, 2765, 2760, 2755,
    2751, 2746, 2741, 2737, 2732, 2727, 2723, 2718, 2713, 2709, 2704, 2699, 2694, 2690, 2685, 2680,
    2675, 2671, 2666, 2661, 2656, 2652, 2647, 2642, 2637, 2632, 2628, 2623, 2618, 2613, 2608, 2603,
    2598, 2594, 2589, 2584, 2579, 2574, 2569, 2564, 2559, 2555, 2550, 2545, 2540, 2535, 2530, 2525,
    2520, 2515, 2510, 2505, 2500, 2495, 2490, 2485, 2480, 2475, 2470, 2465, 2460, 2455, 2450, 2445,
    2440, 2435, 2430, 2425, 2420, 2415, 2410, 2405, 2399, 2394, 2389, 2384, 2379, 2374, 2369, 2364,
    2359, 2353, 2348, 2343, 2338, 2333, 2328, 2322, 2317, 2312, 2307, 2302, 2296, 2291, 2286, 2281,
    2276, 2270, 2265, 2260, 2255, 2249, 2244, 2239, 2234, 2228, 2223, 2218, 2213, 2207, 2202, 2197,
    2191, 2186, 2181, 2175, 2170, 2165, 2159, 2154, 2149, 2143, 2138, 2133, 2127, 2122, 2117, 2111,
    2106, 2100, 2095, 2090, 2084, 2079, 2073, 2068, 2062, 2057, 2052, 2046, 2041, 2035, 2030, 2024,
    2019, 2013, 2008, 2002, 1997, 1992, 1986, 1981, 1975, 1970, 1964, 1958, 1953, 1947, 1942, 1936,
    1931, 1925, 1920, 1914, 1909, 1903, 1898, 1892, 1886, 1881, 1875, 1870, 1864, 1858, 1853, 1847,
    1842, 1836, 1830, 1825, 1819, 1813, 1808, 1802, 1797, 1791, 1785, 1780, 1774, 1768, 1763, 1757,
    1751, 1746, 1740, 1734, 1729, 1723, 1717, 1711, 1706, 1700, 1694, 1689, 1683, 1677, 1671, 1666,
    1660, 1654, 1648, 1643, 1637, 1631, 1625, 1620, 1614, 1608, 1602, 1596, 1591, 1585, 1579, 1573,
    1567, 1562, 1556, 1550, 1544, 1538, 1533, 1527, 1521, 1515, 1509, 1503, 1498, 1492, 1486, 1480,
    1474, 1468, 1462, 1457, 1451, 1445, 1439, 1433, 1427, 1421, 1415, 1409, 1404, 1398, 1392, 1386,
    1380, 1374, 1368, 1362, 1356, 1350, 1344, 1338, 1332, 1327, 1321, 1315, 1309, 1303, 1297, 1291,
    1285, 1279, 1273, 1267, 1261, 1255, 1249, 1243, 1237, 1231, 1225, 1219, 1213, 1207, 1201, 1195,
    1189, 1183, 1177, 1171, 1165, 1159, 1153, 1147, 1141, 1135, 1129, 1123, 1117, 1111, 1105, 1099,
    1092, 1086, 1080, 1074, 1068, 1062, 1056, 1050, 1044, 1038, 1032, 1026, 1020, 1014, 1007, 1001,
    995, 989, 983, 977, 971, 965, 959, 953, 946, 940, 934, 928, 922, 916, 910, 904,
    897, 891, 885, 879, 873, 867, 861, 854, 848, 842, 836, 830, 824, 818, 811, 805,
    799, 793, 787, 781, 774, 768, 762, 756, 750, 744, 737, 731, 725, 719, 713, 706,
    700, 694, 688, 682, 675, 669, 663, 657, 651, 644, 638, 632, 626, 620, 613, 607,
    601, 595, 589, 582, 576, 570, 564, 557, 551, 545, 539, 533, 526, 520, 514, 508,
    501, 495, 489, 483, 476, 470, 464, 458, 451, 445, 439, 433, 426, 420, 414, 408,
    401, 395, 389, 383, 376, 370, 364, 358, 351, 345, 339, 333, 326, 320, 314, 308,
    301, 295, 289, 283, 276, 270, 264, 257, 251, 245, 239, 232, 226, 220, 214, 207,
    201, 195, 188, 182, 176, 170, 163, 157, 151, 144, 138, 132, 126, 119, 113, 107,
    101, 94, 88, 82, 75, 69, 63, 57, 50, 44, 38, 31, 25, 19, 13, 6,
    0, -6, -13, -19, -25, -31, -38, -44, -50, -57, -63, -69, -75, -82, -88, -94,
    -101, -107, -113, -119, -126, -132, -138, -144, -151, -157, -163, -170, -176, -182, -188, -195,
    -201, -207, -214, -220, -226, -232, -239, -245, -251, -257, -264, -270, -276, -283, -289, -295,
    -301, -308, -314, -320, -326, -333, -339, -345, -351, -358, -364, -370, -376, -383, -389, -395,
    -401, -408, -414, -420, -426, -433, -439, -445, -451, -458, -464, -470, -476, -483, -489, -495,
    -501, -508, -514, -520, -526, -533, -539, -545, -551, -557, -564, -570, -576, -582, -589, -595,
    -601, -607, -613, -620, -626, -632, -638, -644, -651, -657, -663, -669, -675, -682, -688, -694,
    -700, -706, -713, -719, -725, -731, -737, -744, -750, -756, -762, -768, -774, -781, -787, -793,
    -799, -805, -811, -818, -824, -830, -836, -842, -848, -854, -861, -867, -873, -879, -885, -891,
    -897, -904, -910, -916, -922, -928, -934, -940, -946, -953, -959, -965, -971, -977, -983, -989,
    -995, -1001, -1007, -1014, -1020, -1026, -1032, -1038, -1044, -1050, -1056, -1062, -1068, -1074, -1080, -1086,
    -1092, -1099, -1105, -1111, -1117, -1123, -1129, -1135, -1141, -1147, -1153, -1159, -1165, -1171, -1177, -1183,
    -1189, -1195, -1201, -1207, -1213, -1219, -1225, -1231, -1237, -1243, -1249, -1255, -1261, -1267, -1273, -1279,
    -1285, -1291, -1297, -1303, -1309, -1315, -1321, -1327, -1332, -1338, -1344, -1350, -1356, -1362, -1368, -1374,
    -1380, -1386, -1392, -1398, -1404, -1409, -1415, -1421, -1427, -1433, -1439, -1445, -1451, -1457, -1462, -1468,
    -1474, -1480, -1486, -1492, -1498, -1503, -1509, -1515, -1521, -1527, -1533, -1538, -1544, -1550, -1556, -1562,
    -1567, -1573, -1579, -1585, -1591, -1596, -1602, -1608, -1614, -1620, -1625, -1631, -1637, -1643, -1648, -1654,
    -1660, -1666, -1671, -1677, -1683, -1689, -1694, -1700, -1706, -1711, -1717, -1723, -1729, -1734, -1740, -1746,
    -1751, -1757, -1763, -1768, -1774, -1780, -1785, -1791, -1797, -1802, -1808, -1813, -1819, -1825, -1830, -1836,
    -1842, -1847, -1853, -1858, -1864, -1870, -1875, -1881, -1886, -1892, -1898, -1903, -1909, -1914, -1920, -1925,
    -1931, -1936, -1942, -1947, -1953, -1958, -1964, -1970, -1975, -1981, -1986, -1992, -1997, -2002, -2008, -2013,
    -2019, -2024, -2030, -2035, -2041, -2046, -2052, -2057, -2062, -2068, -2073, -2079, -2084, -2090, -2095, -2100,
    -2106, -2111, -2117, -2122, -2127, -2133, -2138, -2143, -2149, -2154, -2159, -2165, -2170, -2175, -2181, -2186,
    -2191, -2197, -2202, -2207, -2213, -2218, -2223, -2228, -2234, -2239, -2244, -2249, -2255, -2260, -2265, -2270,
    -2276, -2281, -2286, -2291, -2296, -2302, -2307, -2312, -2317, -2322, -2328, -2333, -2338, -2343, -2348, -2353,
    -2359, -2364, -2369, -2374, -2379, -2384, -2389, -2394, -2399, -2405, -2410, -2415, -2420, -2425, -2430, -2435,
    -2440, -2445, -2450, -2455, -2460, -2465, -2470, -2475, -2480, -2485, -2490, -2495, -2500, -2505, -2510, -2515,
    -2520, -2525, -2530, -2535, -2540, -2545, -2550, -2555, -2559, -2564, -2569, -2574, -2579, -2584, -2589, -2594,
    -2598, -2603, -2608, -2613, -2618, -2623, -2628, -2632, -2637, -2642, -2647, -2652, -2656, -2661, -2666, -2671,
    -2675, -2680, -2685, -2690, -2694, -2699, -2704, -2709, -2713, -2718, -2723, -2727, -2732, -2737, -2741, -2746,
    -2751, -2755, -2760, -2765, -2769, -2774, -2779, -2783, -2788, -2792, -2797, -2802, -2806, -2811, -2815, -2820,
    -2824, -2829, -2833, -2838, -2843, -2847, -2852, -2856, -2861, -2865, -2870, -2874, -2878, -2883, -2887, -2892,
    -2896, -2901, -2905, -2910, -2914, -2918, -2923, -2927, -2932, -2936, -2940, -2945, -2949, -2953, -2958, -2962,
    -2967, -2971, -2975, -2979, -2984, -2988, -2992, -2997, -3001, -3005, -3009, -3014, -3018, -3022, -3026, -3031,
    -3035, -3039, -3043, -3048, -3052, -3056, -3060, -3064, -3068, -3073, -3077, -3081, -3085, -3089, -3093, -3097,
    -3102, -3106, -3110, -3114, -3118, -3122, -3126, -3130, -3134, -3138, -3142, -3146, -3150, -3154, -3158, -3162,
    -3166, -3170, -3174, -3178, -3182, -3186, -3190, -3194, -3198, -3202, -3206, -3210, -3214, -3217, -3221, -3225,
    -3229, -3233, -3237, -3241, -3244, -3248, -3252, -3256, -3260, -3264, -3267, -3271, -3275, -3279, -3282, -3286,
    -3290, -3294, -3297, -3301, -3305, -3309, -3312, -3316, -3320, -3323, -3327, -3331, -3334, -3338, -3342, -3345,
    -3349, -3352, -3356, -3360, -3363, -3367, -3370, -3374, -3378, -3381, -3385, -3388, -3392, -3395, -3399, -3402,
    -3406, -3409, -3413, -3416, -3420, -3423, -3426, -3430, -3433, -3437, -3440, -3444, -3447, -3450, -3454, -3457,
    -3461, -3464, -3467, -3471, -3474, -3477, -3481, -3484, -3487, -3490, -3494, -3497, -3500, -3504, -3507, -3510,
    -3513, -3516, -3520, -3523, -3526, -3529, -3532, -3536, -3539, -3542, -3545, -3548, -3551, -3555, -3558, -3561,
    -3564, -3567, -3570, -3573, -3576, -3579, -3582, -3585, -3588, -3591, -3594, -3597, -3600, -3603, -3606, -3609,
    -3612, -3615, -3618, -3621, -3624, -3627, -3630, -3633, -3636, -3639, -3642, -3644, -3647, -3650, -3653, -3656,
    -3659, -3661, -3664, -3667, -3670, -3673, -3675, -3678, -3681, -3684, -3686, -3689, -3692, -3695, -3697, -3700,
    -3703, -3705, -3708, -3711, -3713, -3716, -3719, -3721, -3724, -3727, -3729, -3732, -3734, -3737, -3739, -3742,
    -3745, -3747, -3750, -3752, -3755, -3757, -3760, -3762, -3765, -3767, -3770, -3772, -3775, -3777, -3779, -3782,
    -3784, -3787, -3789, -3791, -3794, -3796, -3798, -3801, -3803, -3805, -3808, -3810, -3812, -3815, -3817, -3819,
    -3822, -3824, -3826, -3828, -3831, -3833, -3835, -3837, -3839, -3842, -3844, -3846, -3848, -3850, -3852, -3854,
    -3857, -3859, -3861, -3863, -3865, -3867, -3869, -3871, -3873, -3875, -3877, -3879, -3881, -3883, -3885, -3887,
    -3889, -3891, -3893, -3895, -3897, -3899, -3901, -3903, -3905, -3907, -3909, -3910, -3912, -3914, -3916, -3918,
    -3920, -3921, -3923, -3925, -3927, -3929, -3930, -3932, -3934, -3936, -3937, -3939, -3941, -3943, -3944, -3946,
    -3948, -3949, -3951, -3953, -3954, -3956, -3958, -3959, -3961, -3962, -3964, -3965, -3967, -3969, -3970, -3972,
    -3973, -3975, -3976, -3978, -3979, -3981, -3982, -3984, -3985, -3987, -3988, -3989, -3991, -3992, -3994, -3995,
    -3996, -3998, -3999, -4001, -4002, -4003, -4005, -4006, -4007, -4008, -4010, -4011, -4012, -4014, -4015, -4016,
    -4017, -4019, -4020, -4021, -4022, -4023, -4024, -4026, -4027, -4028, -4029, -4030, -4031, -4032, -4034, -4035,
    -4036, -4037, -4038, -4039, -4040, -4041, -4042, -4043, -4044, -4045, -4046, -4047, -4048, -4049, -4050, -4051,
    -4052, -4053, -4053, -4054, -4055, -4056, -4057, -4058, -4059, -4060, -4060, -4061, -4062, -4063, -4064, -4064,
    -4065, -4066, -4067, -4067, -4068, -4069, -4070, -4070, -4071, -4072, -4072, -4073, -4074, -4074, -4075, -4076,
    -4076, -4077, -4077, -4078, -4079, -4079, -4080, -4080, -4081, -4081, -4082, -4082, -4083, -4083, -4084, -4084,
    -4085, -4085, -4086, -4086, -4087, -4087, -4088, -4088, -4088, -4089, -4089, -4089, -4090, -4090, -4090, -4091,
    -4091, -4091, -4092, -4092, -4092, -4092, -4093, -4093, -4093, -4093, -4094, -4094, -4094, -4094, -4094, -4095,
    -4095, -4095, -4095, -4095, -4095, -4095, -4096, -4096, -4096, -4096, -4096, -4096, -4096, -4096, -4096, -4096,
    -4096, -4096, -4096, -4096, -4096, -4096, -4096, -4096, -4096, -4096, -4096, -4095, -4095, -4095, -4095, -4095,
    -4095, -4095, -4094, -4094, -4094, -4094, -4094, -4093, -4093, -4093, -4093, -4092, -4092, -4092, -4092, -4091,
    -4091, -4091, -4090, -4090, -4090, -4089, -4089, -4089, -4088, -4088, -4088, -4087, -4087, -4086, -4086, -4085,
    -4085, -4084, -4084, -4083, -4083, -4082, -4082, -4081, -4081, -4080, -4080, -4079, -4079, -4078, -4077, -4077,
    -4076, -4076, -4075, -4074, -4074, -4073, -4072, -4072, -4071, -4070, -4070, -4069, -4068, -4067, -4067, -4066,
    -4065, -4064, -4064, -4063, -4062, -4061, -4060, -4060, -4059, -4058, -4057, -4056, -4055, -4054, -4053, -4053,
    -4052, -4051, -4050, -4049, -4048, -4047, -4046, -4045, -4044, -4043, -4042, -4041, -4040, -4039, -4038, -4037,
    -4036, -4035, -4034, -4032, -4031, -4030, -4029, -4028, -4027, -4026, -4024, -4023, -4022, -4021, -4020, -4019,
    -4017, -4016, -4015, -4014, -4012, -4011, -4010, -4008, -4007, -4006, -4005, -4003, -4002, -4001, -3999, -3998,
    -3996, -3995, -3994, -3992, -3991, -3989, -3988, -3987, -3985, -3984, -3982, -3981, -3979, -3978, -3976, -3975,
    -3973, -3972, -3970, -3969, -3967, -3965, -3964, -3962, -3961, -3959, -3958, -3956, -3954, -3953, -3951, -3949,
    -3948, -3946, -3944, -3943, -3941, -3939, -3937, -3936, -3934, -3932, -3930, -3929, -3927, -3925, -3923, -3921,
    -3920, -3918, -3916, -3914, -3912, -3910, -3909, -3907, -3905, -3903, -3901, -3899, -3897, -3895, -3893, -3891,
    -3889, -3887, -3885, -3883, -3881, -3879, -3877, -3875, -3873, -3871, -3869, -3867, -3865, -3863, -3861, -3859,
    -3857, -3854, -3852, -3850, -3848, -3846, -3844, -3842, -3839, -3837, -3835, -3833, -3831, -3828, -3826, -3824,
    -3822, -3819, -3817, -3815, -3812, -3810, -3808, -3805, -3803, -3801, -3798, -3796, -3794, -3791, -3789, -3787,
    -3784, -3782, -3779, -3777, -3775, -3772, -3770, -3767, -3765, -3762, -3760, -3757, -3755, -3752, -3750, -3747,
    -3745, -3742, -3739, -3737, -3734, -3732, -3729, -3727, -3724, -3721, -3719, -3716, -3713, -3711, -3708, -3705,
    -3703, -3700, -3697, -3695, -3692, -3689, -3686, -3684, -3681, -3678, -3675, -3673, -3670, -3667, -3664, -3661,
    -3659, -3656, -3653, -3650, -3647, -3644, -3642, -3639, -3636, -3633, -3630, -3627, -3624, -3621, -3618, -3615,
    -3612, -3609, -3606, -3603, -3600, -3597, -3594, -3591, -3588, -3585, -3582, -3579, -3576, -3573, -3570, -3567,
    -3564, -3561, -3558, -3555, -3551, -3548, -3545, -3542, -3539, -3536, -3532, -3529, -3526, -3523, -3520, -3516,
    -3513, -3510, -3507, -3504, -3500, -3497, -3494, -3490, -3487, -3484, -3481, -3477, -3474, -3471, -3467, -3464,
    -3461, -3457, -3454, -3450, -3447, -3444, -3440, -3437, -3433, -3430, -3426, -3423, -3420, -3416, -3413, -3409,
    -3406, -3402, -3399, -3395, -3392, -3388, -3385, -3381, -3378, -3374, -3370, -3367, -3363, -3360, -3356, -3352,
    -3349, -3345, -3342, -3338, -3334, -3331, -3327, -3323, -3320, -3316, -3312, -3309, -3305, -3301, -3297, -3294,
    -3290, -3286, -3282, -3279, -3275, -3271, -3267, -3264, -3260, -3256, -3252, -3248, -3244, -3241, -3237, -3233,
    -3229, -3225, -3221, -3217, -3214, -3210, -3206, -3202, -3198, -3194, -3190, -3186, -3182, -3178, -3174, -3170,
    -3166, -3162, -3158, -3154, -3150, -3146, -3142, -3138, -3134, -3130, -3126, -3122, -3118, -3114, -3110, -3106,
    -3102, -3097, -3093, -3089, -3085, -3081, -3077, -3073, -3068, -3064, -3060, -3056, -3052, -3048, -3043, -3039,
    -3035, -3031, -3026, -3022, -3018, -3014, -3009, -3005, -3001, -2997, -2992, -2988, -2984, -2979, -2975, -2971,
    -2967, -2962, -2958, -2953, -2949, -2945, -2940, -2936, -2932, -2927, -2923, -2918, -2914, -2910, -2905, -2901,
    -2896, -2892, -2887, -2883, -2878, -2874, -2870, -2865, -2861, -2856, -2852, -2847, -2843, -2838, -2833, -2829,
    -2824, -2820, -2815, -2811, -2806, -2802, -2797, -2792, -2788, -2783, -2779, -2774, -2769, -2765, -2760, -2755,
    -2751, -2746, -2741, -2737, -2732, -2727, -2723, -2718, -2713, -2709, -2704, -2699, -2694, -2690, -2685, -2680,
    -2675, -2671, -2666, -2661, -2656, -2652, -2647, -2642, -2637, -2632, -2628, -2623, -2618, -2613, -2608, -2603,
    -2598, -2594, -2589, -2584, -2579, -2574, -2569, -2564, -2559, -2555, -2550, -2545, -2540, -2535, -2530, -2525,
    -2520, -2515, -2510, -2505, -2500, -2495, -2490, -2485, -2480, -2475, -2470, -2465, -2460, -2455, -2450, -2445,
    -2440, -2435, -2430, -2425, -2420, -2415, -2410, -2405, -2399, -2394, -2389, -2384, -2379, -2374, -2369, -2364,
    -2359, -2353, -2348, -2343, -2338, -2333, -2328, -2322, -2317, -2312, -2307, -2302, -2296, -2291, -2286, -2281,
    -2276, -2270, -2265, -2260, -2255, -2249, -2244, -2239, -2234, -2228, -2223, -2218, -2213, -2207, -2202, -2197,
    -2191, -2186, -2181, -2175, -2170, -2165, -2159, -2154, -2149, -2143, -2138, -2133, -2127, -2122, -2117, -2111,
    -2106, -2100, -2095, -2090, -2084, -2079, -2073, -2068, -2062, -2057, -2052, -2046, -2041, -2035, -2030, -2024,
    -2019, -2013, -2008, -2002, -1997, -1992, -1986, -1981, -1975, -1970, -1964, -1958, -1953, -1947, -1942, -1936,
    -1931, -1925, -1920, -1914, -1909, -1903, -1898, -1892, -1886, -1881, -1875, -1870, -1864, -1858, -1853, -1847,
    -1842, -1836, -1830, -1825, -1819, -1813, -1808, -1802, -1797, -1791, -1785, -1780, -1774, -1768, -1763, -1757,
    -1751, -1746, -1740, -1734, -1729, -1723, -1717, -1711, -1706, -1700, -1694, -1689, -1683, -1677, -1671, -1666,
    -1660, -1654, -1648, -1643, -1637, -1631, -1625, -1620, -1614, -1608, -1602, -1596, -1591, -1585, -1579, -1573,
    -1567, -1562, -1556, -1550, -1544, -1538, -1533, -1527, -1521, -1515, -1509, -1503, -1498, -1492, -1486, -1480,
    -1474, -1468, -1462, -1457, -1451, -1445, -1439, -1433, -1427, -1421, -1415, -1409, -1404, -1398, -1392, -1386,
    -1380, -1374, -1368, -1362, -1356, -1350, -1344, -1338, -1332, -1327, -1321, -1315, -1309, -1303, -1297, -1291,
    -1285, -1279, -1273, -1267, -1261, -1255, -1249, -1243, -1237, -1231, -1225, -1219, -1213, -1207, -1201, -1195,
    -1189, -1183, -1177, -1171, -1165, -1159, -1153, -1147, -1141, -1135, -1129, -1123, -1117, -1111, -1105, -1099,
    -1092, -1086, -1080, -1074, -1068, -1062, -1056, -1050, -1044, -1038, -1032, -1026, -1020, -1014, -1007, -1001,
    -995, -989, -983, -977, -971, -965, -959, -953, -946, -940, -934, -928, -922, -916, -910, -904,
    -897, -891, -885, -879, -873, -867, -861, -854, -848, -842, -836, -830, -824, -818, -811, -805,
    -799, -793, -787, -781, -774, -768, -762, -756, -750, -744, -737, -731, -725, -719, -713, -706,
    -700, -694, -688, -682, -675, -669, -663, -657, -651, -644, -638, -632, -626, -620, -613, -607,
    -601, -595, -589, -582, -576, -570, -564, -557, -551, -545, -539, -533, -526, -520, -514, -508,
    -501, -495, -489, -483, -476, -470, -464, -458, -451, -445, -439, -433, -426, -420, -414, -408,
    -401, -395, -389, -383, -376, -370, -364, -358, -351, -345, -339, -333, -326, -320, -314, -308,
    -301, -295, -289, -283, -276, -270, -264, -257, -251, -245, -239, -232, -226, -220, -214, -207,
    -201, -195, -188, -182, -176, -170, -163, -157, -151, -144, -138, -132, -126, -119, -113, -107,
    -101, -94, -88, -82, -75, -69, -63, -57, -50, -44, -38, -31, -25, -19, -13, -6,
};
static re3c_u8 nibble_at(const re3c_u8*p,re3c_u32 n){re3c_u8 b=p[n>>1];return (re3c_u8)((n&1u)?(b>>4):(b&15u));}
static re3c_i16 angle12(const re3c_u8*p,re3c_u32 n){return (re3c_i16)((re3c_u16)nibble_at(p,n)|((re3c_u16)nibble_at(p,n+1u)<<4)|((re3c_u16)nibble_at(p,n+2u)<<8));}
int re3c_decode_pose_by_frame(const re3c_model*m,re3c_u32 bi,re3c_u32 frame,re3c_pose*out){const re3c_anim_bank*b;const re3c_section*s;static const re3c_u8 ms[5]={3,5,7,10,12};re3c_u8 buf[8+RE3C_MAX_PARTS*5u];re3c_u32 off,i,need;int r;if(!m||!out||bi>=5u)return RE3C_ERR_BAD_ARGUMENT;b=&m->banks[bi];if(!b->present)return RE3C_ERR_NOT_PRESENT;if(frame>=b->physical_frame_count)return RE3C_ERR_RANGE;s=&m->sections[ms[bi]];off=b->frame_offset+frame*b->frame_size;if(!insec(s,off,b->frame_size))return RE3C_ERR_RANGE;need=b->frame_size;if(need>sizeof(buf))return RE3C_ERR_OVERFLOW;r=src_read(&m->source,s->offset+off,buf,need);if(r!=RE3C_OK)return r;memset(out,0,sizeof(*out));out->joint_count=b->joint_count;out->speed=posfx(rds16(buf),rds16(buf+2),rds16(buf+4));out->origin_y=(re3c_i32)rds16(buf+6)*RE3C_FX_ONE;if(8u+((re3c_u32)b->joint_count*9u+1u)/2u>need)return RE3C_ERR_BAD_FORMAT;for(i=0;i<b->joint_count;i++){re3c_u32 n=i*9u;out->rot_x[i]=angle12(buf+8,n);out->rot_y[i]=angle12(buf+8,n+3u);out->rot_z[i]=angle12(buf+8,n+6u);}return RE3C_OK;}
int re3c_resolve_key(const re3c_model*m,re3c_u32 bi,re3c_u32 ci,re3c_u32 ki,re3c_key_resolution*out){const re3c_anim_bank*b;const re3c_clip*c;const re3c_key*k;re3c_u32 gi,p,n;if(!m||!out||bi>=5u)return RE3C_ERR_BAD_ARGUMENT;b=&m->banks[bi];if(!b->present)return RE3C_ERR_NOT_PRESENT;if(ci>=b->clip_count)return RE3C_ERR_RANGE;c=&b->clips[ci];if(ki>=c->key_count)return RE3C_ERR_RANGE;gi=c->first_key+ki;k=&b->keys[gi];memset(out,0,sizeof(*out));out->key_kind=k->key_kind;out->timeline_only=k->timeline_only;out->pose_available=k->pose_available;out->paired_bank=k->paired_bank;out->paired_frame=RE3C_FRAME_NONE;if(k->key_kind==RE3C_KEY_EXPLICIT){out->frame_a=k->frame_index;out->frame_b=k->frame_index;if(k->timeline_only&&k->paired_bank<5u){const re3c_anim_bank*pb=&m->banks[k->paired_bank];if(ci<pb->clip_count&&ki<pb->clips[ci].key_count)out->paired_frame=pb->keys[pb->clips[ci].first_key+ki].frame_index;}return k->resolvable?RE3C_OK:RE3C_ERR_RANGE;}
p=ki;while(p>0u){p--;if(b->keys[c->first_key+p].key_kind==RE3C_KEY_EXPLICIT)break;}if(b->keys[c->first_key+p].key_kind!=RE3C_KEY_EXPLICIT)return RE3C_ERR_BAD_FORMAT;n=ki;while(++n<c->key_count)if(b->keys[c->first_key+n].key_kind==RE3C_KEY_EXPLICIT)break;if(n>=c->key_count)return RE3C_ERR_BAD_FORMAT;out->frame_a=b->keys[c->first_key+p].frame_index;out->frame_b=b->keys[c->first_key+n].frame_index;out->factor_q8=k->low;if(out->frame_a>=b->physical_frame_count||out->frame_b>=b->physical_frame_count)return RE3C_ERR_RANGE;out->pose_available=1u;return RE3C_OK;}
static re3c_i16 lerp_angle(re3c_i16 a,re3c_i16 b,re3c_u8 t){re3c_i32 d=((re3c_i32)b-(re3c_i32)a)&4095;if(d>2048)d-=4096;return (re3c_i16)(((re3c_i32)a+(d*(re3c_i32)t)/256)&4095);}
int re3c_decode_pose_by_key(const re3c_model*m,re3c_u32 bi,re3c_u32 ci,re3c_u32 ki,re3c_pose*out){re3c_key_resolution q;re3c_pose a,b;re3c_u32 i;int r;if(!out)return RE3C_ERR_BAD_ARGUMENT;r=re3c_resolve_key(m,bi,ci,ki,&q);if(r!=RE3C_OK)return r;if(!q.pose_available)return RE3C_ERR_NOT_PRESENT;if(q.key_kind==RE3C_KEY_EXPLICIT)return re3c_decode_pose_by_frame(m,bi,q.frame_a,out);r=re3c_decode_pose_by_frame(m,bi,q.frame_a,&a);if(r!=RE3C_OK)return r;r=re3c_decode_pose_by_frame(m,bi,q.frame_b,&b);if(r!=RE3C_OK)return r;*out=a;out->speed.x=a.speed.x+(re3c_i32)(((b.speed.x-a.speed.x)*(re3c_i32)q.factor_q8)/256);out->speed.y=a.speed.y+(re3c_i32)(((b.speed.y-a.speed.y)*(re3c_i32)q.factor_q8)/256);out->speed.z=a.speed.z+(re3c_i32)(((b.speed.z-a.speed.z)*(re3c_i32)q.factor_q8)/256);out->origin_y=a.origin_y+(re3c_i32)(((b.origin_y-a.origin_y)*(re3c_i32)q.factor_q8)/256);for(i=0;i<a.joint_count;i++){out->rot_x[i]=lerp_angle(a.rot_x[i],b.rot_x[i],q.factor_q8);out->rot_y[i]=lerp_angle(a.rot_y[i],b.rot_y[i],q.factor_q8);out->rot_z[i]=lerp_angle(a.rot_z[i],b.rot_z[i],q.factor_q8);}return RE3C_OK;}
static re3c_fx csin(re3c_i16 a){return sin_q12[((re3c_u16)a)&4095u];}
static re3c_fx ccos(re3c_i16 a){return sin_q12[(((re3c_u16)a)+1024u)&4095u];}
static void ident(re3c_mat34fx*m){memset(m,0,sizeof(*m));m->m00=m->m11=m->m22=RE3C_FX_ONE;}
static re3c_mat34fx mulm(const re3c_mat34fx*a,const re3c_mat34fx*b){re3c_mat34fx r;r.m00=re3c_fx_mul(a->m00,b->m00)+re3c_fx_mul(a->m01,b->m10)+re3c_fx_mul(a->m02,b->m20);r.m01=re3c_fx_mul(a->m00,b->m01)+re3c_fx_mul(a->m01,b->m11)+re3c_fx_mul(a->m02,b->m21);r.m02=re3c_fx_mul(a->m00,b->m02)+re3c_fx_mul(a->m01,b->m12)+re3c_fx_mul(a->m02,b->m22);r.m03=re3c_fx_mul(a->m00,b->m03)+re3c_fx_mul(a->m01,b->m13)+re3c_fx_mul(a->m02,b->m23)+a->m03;r.m10=re3c_fx_mul(a->m10,b->m00)+re3c_fx_mul(a->m11,b->m10)+re3c_fx_mul(a->m12,b->m20);r.m11=re3c_fx_mul(a->m10,b->m01)+re3c_fx_mul(a->m11,b->m11)+re3c_fx_mul(a->m12,b->m21);r.m12=re3c_fx_mul(a->m10,b->m02)+re3c_fx_mul(a->m11,b->m12)+re3c_fx_mul(a->m12,b->m22);r.m13=re3c_fx_mul(a->m10,b->m03)+re3c_fx_mul(a->m11,b->m13)+re3c_fx_mul(a->m12,b->m23)+a->m13;r.m20=re3c_fx_mul(a->m20,b->m00)+re3c_fx_mul(a->m21,b->m10)+re3c_fx_mul(a->m22,b->m20);r.m21=re3c_fx_mul(a->m20,b->m01)+re3c_fx_mul(a->m21,b->m11)+re3c_fx_mul(a->m22,b->m21);r.m22=re3c_fx_mul(a->m20,b->m02)+re3c_fx_mul(a->m21,b->m12)+re3c_fx_mul(a->m22,b->m22);r.m23=re3c_fx_mul(a->m20,b->m03)+re3c_fx_mul(a->m21,b->m13)+re3c_fx_mul(a->m22,b->m23)+a->m23;return r;}
static re3c_mat34fx rotxyz(re3c_i16 x,re3c_i16 y,re3c_i16 z){re3c_mat34fx rx,ry,rz,t;re3c_fx sx=csin(x),cx=ccos(x),sy=csin(y),cy=ccos(y),sz=csin(z),cz=ccos(z);ident(&rx);ident(&ry);ident(&rz);rx.m11=cx;rx.m12=-sx;rx.m21=sx;rx.m22=cx;ry.m00=cy;ry.m02=sy;ry.m20=-sy;ry.m22=cy;rz.m00=cz;rz.m01=-sz;rz.m10=sz;rz.m11=cz;t=mulm(&rx,&ry);return mulm(&t,&rz);}
int re3c_mat34_transform_point(const re3c_mat34fx*m,const re3c_vec3fx*p,re3c_vec3fx*out){if(!m||!p||!out)return RE3C_ERR_BAD_ARGUMENT;out->x=re3c_fx_mul(m->m00,p->x)+re3c_fx_mul(m->m01,p->y)+re3c_fx_mul(m->m02,p->z)+m->m03;out->y=re3c_fx_mul(m->m10,p->x)+re3c_fx_mul(m->m11,p->y)+re3c_fx_mul(m->m12,p->z)+m->m13;out->z=re3c_fx_mul(m->m20,p->x)+re3c_fx_mul(m->m21,p->y)+re3c_fx_mul(m->m22,p->z)+m->m23;return RE3C_OK;}
int re3c_build_joint_matrices(const re3c_anim_bank*b,const re3c_pose*p,re3c_u32 mode,re3c_mat34fx out[RE3C_MAX_PARTS]){re3c_u32 i,pass,done[RE3C_MAX_PARTS];if(!b||!p||!out)return RE3C_ERR_BAD_ARGUMENT;if(!b->present||!b->joint_count)return RE3C_ERR_NOT_PRESENT;
 /* A bank may have keys but NO native/inherited joint tree (e.g. damage
  * banks with 15 joints against a 16-joint normal bank). Never calculate
  * matrices from its zero-filled placeholder joints. */
 if(b->hierarchy_source==255u)return RE3C_ERR_NOT_PRESENT;
 if(p->joint_count!=b->joint_count)return RE3C_ERR_BAD_FORMAT;
 for(i=0;i<b->joint_count;i++){if(b->joints[i].parent>=0&&(re3c_u32)b->joints[i].parent>=b->joint_count)return RE3C_ERR_BAD_FORMAT;}
 memset(done,0,sizeof(done));for(pass=0;pass<=b->joint_count;pass++){for(i=0;i<b->joint_count;i++){re3c_i16 par=b->joints[i].parent;re3c_mat34fx l;if(done[i])continue;if(par>=0&&!done[(re3c_u32)par])continue;l=rotxyz(p->rot_x[i],p->rot_y[i],p->rot_z[i]);if(par<0){if(mode==RE3C_ROOT_FRAME_SPACE){l.m03=p->speed.x;l.m13=p->speed.y+p->origin_y;l.m23=p->speed.z;}else{l.m03=l.m13=l.m23=0;}out[i]=l;}else{l.m03=b->joints[i].local_pos.x;l.m13=b->joints[i].local_pos.y;l.m23=b->joints[i].local_pos.z;out[i]=mulm(&out[(re3c_u32)par],&l);}done[i]=1u;}}for(i=0;i<b->joint_count;i++)if(!done[i])return RE3C_ERR_NOT_PRESENT;return RE3C_OK;}

/* ------------------------------------------------------------------------- */
/* Binary01 morph + DAT ownership/binding recovery path.                      */
/* These routines reconstruct the corpus-validated v0.12/v0.13 behavior.     */
/* ------------------------------------------------------------------------- */
static int src_u8(const re3c_source*s,re3c_u32 o,re3c_u8*v){return src_read(s,o,v,1u);}
static re3c_u32 popcount32(re3c_u32 v){re3c_u32 n=0u;while(v){n+=v&1u;v>>=1;}return n;}

int re3c_parse_binary01_morph(re3c_model*m){
 const re3c_section*s;re3c_morph_bank*mb;re3c_u8 h[16];re3c_u32 mask,end,rest,cursor,bit,ti,ntracks;int r;
 if(!m)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 mb=&m->binary01_morph;memset(mb,0,sizeof(*mb));s=&m->sections[RE3C_SEC_BINARY01];
 if(!s->present||s->size<=4u)return RE3C_OK;
 if(s->size<12u)return RE3C_ERR_BAD_FORMAT;
 r=src_read(&m->source,s->offset,h,12u);if(r!=RE3C_OK)return r;mask=rd32(h);end=rd32(h+4);rest=rd32(h+8);
 ntracks=popcount32(mask);if(!ntracks||ntracks>RE3C_MAX_MORPH_TRACKS)return RE3C_ERR_BAD_FORMAT;
 if(end<12u||end>s->size)return RE3C_ERR_BAD_FORMAT;
 if(rest!=align4((re3c_u32)m->banks[RE3C_BANK_A].joint_count*6u))return RE3C_ERR_BAD_FORMAT;
 mb->present=1u;mb->part_mask=mask;mb->records_end=end;mb->rest_pos_size=rest;mb->track_count=(re3c_u16)ntracks;
 cursor=12u;ti=0u;
 for(bit=0u;bit<32u;bit++){
  re3c_u32 off,stride,extra,resbytes,ri;re3c_morph_track*t;const re3c_mesh_part*p;
  if(!(mask&(1u<<bit)))continue;
  if(bit>=m->body.part_count||ti>=RE3C_MAX_MORPH_TRACKS)return RE3C_ERR_BAD_FORMAT;
  if(!insec(s,cursor,12u))return RE3C_ERR_BAD_FORMAT;
  r=src_read(&m->source,s->offset+cursor,h,12u);if(r!=RE3C_OK)return r;
  off=rd32(h);stride=rd32(h+4);extra=rd32(h+8);if(extra>65534u)return RE3C_ERR_OVERFLOW;
  resbytes=extra*4u;if(!insec(s,cursor+12u,resbytes))return RE3C_ERR_BAD_FORMAT;
  p=&m->body.parts[bit];if(stride!=align4((re3c_u32)p->vertex_count*6u))return RE3C_ERR_BAD_FORMAT;
  if(off>=s->size||!insec(s,off,(extra+1u)*stride))return RE3C_ERR_BAD_FORMAT;
  t=&mb->tracks[ti];memset(t,0,sizeof(*t));t->present=1u;t->part_index=(re3c_u8)bit;t->vertex_count=p->vertex_count;
  t->frame_count=(re3c_u16)(extra+1u);t->reserved_word_count=(re3c_u16)extra;t->data_offset=off;t->frame_stride=stride;t->reserved_offset=cursor+12u;
  for(ri=0u;ri<extra;ri++){re3c_u32 z;r=src_u32(&m->source,s->offset+cursor+12u+ri*4u,&z);if(r!=RE3C_OK)return r;if(z)t->reserved_nonzero_count++;}
  cursor+=12u+resbytes;ti++;
 }
 if(cursor!=end)return RE3C_ERR_BAD_FORMAT;
 if(!insec(s,end,rest))return RE3C_ERR_BAD_FORMAT;
 /* The rest-position copy must match Normal EMR local positions byte-for-byte. */
 for(bit=0u;bit<m->banks[RE3C_BANK_A].joint_count;bit++){
  re3c_i16 x,y,z;r=src_i16(&m->source,s->offset+end+bit*6u,&x);if(r!=RE3C_OK)return r;
  r=src_i16(&m->source,s->offset+end+bit*6u+2u,&y);if(r!=RE3C_OK)return r;r=src_i16(&m->source,s->offset+end+bit*6u+4u,&z);if(r!=RE3C_OK)return r;
  if(m->banks[RE3C_BANK_A].joints[bit].local_pos.x!=(re3c_i32)x*RE3C_FX_ONE||m->banks[RE3C_BANK_A].joints[bit].local_pos.y!=(re3c_i32)y*RE3C_FX_ONE||m->banks[RE3C_BANK_A].joints[bit].local_pos.z!=(re3c_i32)z*RE3C_FX_ONE)return RE3C_ERR_BAD_FORMAT;
 }
 mb->validated=1u;return RE3C_OK;
}
int re3c_morph_track_for_part(const re3c_model*m,re3c_u32 part,re3c_u32*out){re3c_u32 i;if(!m||!out)return RE3C_ERR_BAD_ARGUMENT;if(!m->binary01_morph.present)return RE3C_ERR_NOT_PRESENT;for(i=0u;i<m->binary01_morph.track_count;i++)if(m->binary01_morph.tracks[i].present&&(re3c_u32)m->binary01_morph.tracks[i].part_index==part){*out=i;return RE3C_OK;}return RE3C_ERR_NOT_PRESENT;}
int re3c_morph_vertex(const re3c_model*m,re3c_u32 part,re3c_u32 frame,re3c_u32 vertex,re3c_vec3fx*out){re3c_u32 ti,off;re3c_i16 x,y,z;const re3c_morph_track*t;const re3c_section*s;int r;if(!m||!out)return RE3C_ERR_BAD_ARGUMENT;r=re3c_morph_track_for_part(m,part,&ti);if(r!=RE3C_OK)return r;t=&m->binary01_morph.tracks[ti];if(frame>=t->frame_count||vertex>=t->vertex_count)return RE3C_ERR_RANGE;s=&m->sections[RE3C_SEC_BINARY01];off=t->data_offset+frame*t->frame_stride+vertex*6u;if(!insec(s,off,6u))return RE3C_ERR_RANGE;r=src_i16(&m->source,s->offset+off,&x);if(r!=RE3C_OK)return r;r=src_i16(&m->source,s->offset+off+2u,&y);if(r!=RE3C_OK)return r;r=src_i16(&m->source,s->offset+off+4u,&z);if(r!=RE3C_OK)return r;*out=posfx(x,y,z);return RE3C_OK;}
int re3c_decode_parts_set(const re3c_u8*p,size_t n,re3c_parts_set_cmd*out){if(!p||!out)return RE3C_ERR_BAD_ARGUMENT;if(n<6u)return RE3C_ERR_RANGE;if(p[0]!=0x2Au)return RE3C_ERR_BAD_FORMAT;out->opcode=p[0];out->align=p[1];out->id=p[2];out->type=p[3];out->value=rds16(p+4);return RE3C_OK;}

static int parse_owner_stream(re3c_model*m,const re3c_section*s,re3c_u32 li,re3c_u16 raw,re3c_u16 joints,re3c_i16 owner[256],re3c_u8*present,re3c_u16*resolved,re3c_u16*count){re3c_u32 stream,assigned,i;int r;if(!m||!s||!owner||!present||!resolved||!count)return RE3C_ERR_BAD_ARGUMENT;for(i=0u;i<256u;i++)owner[i]=-1;*present=0u;*resolved=0u;*count=0u;if(!raw)return RE3C_OK;stream=8u+li*8u+(re3c_u32)raw;if(stream>=s->size||stream>65535u)return RE3C_ERR_BAD_FORMAT;*present=1u;*resolved=(re3c_u16)stream;assigned=0u;while(stream<s->size){re3c_u8 jid,tc;r=src_u8(&m->source,s->offset+stream,&jid);if(r!=RE3C_OK)return r;stream++;if(jid==0xFFu){*count=(re3c_u16)assigned;return RE3C_OK;}if((re3c_u32)jid>=joints||stream>=s->size)return RE3C_ERR_BAD_FORMAT;r=src_u8(&m->source,s->offset+stream,&tc);if(r!=RE3C_OK)return r;stream++;if(!insec(s,stream,(re3c_u32)tc*3u))return RE3C_ERR_BAD_FORMAT;for(i=0u;i<(re3c_u32)tc*3u;i++){re3c_u8 v;r=src_u8(&m->source,s->offset+stream+i,&v);if(r!=RE3C_OK)return r;if(v==0xFFu)continue;if(owner[v]<0){owner[v]=(re3c_i16)jid;assigned++;}else if(owner[v]!=(re3c_i16)jid)return RE3C_ERR_BAD_FORMAT;}stream+=(re3c_u32)tc*3u;}return RE3C_ERR_BAD_FORMAT;}
static int scan_seam_req(re3c_model*m,const re3c_section*s,re3c_bind_bank*b,re3c_u32 li){re3c_bind_limb*l;re3c_u32 stream,maxv=0u;int have=0,r;l=&b->limbs[li];l->seam_required_vertex_count=0u;if(!l->vertex_bind_present)return RE3C_OK;stream=l->seam_stream;while(stream<s->size){re3c_u8 first,raw[8];re3c_u32 k;r=src_u8(&m->source,s->offset+stream,&first);if(r!=RE3C_OK)return r;if(first==0xFFu){if(have)l->seam_required_vertex_count=(re3c_u16)(maxv+1u);return RE3C_OK;}if(!insec(s,stream,8u))return RE3C_ERR_BAD_FORMAT;r=src_read(&m->source,s->offset+stream,raw,8u);if(r!=RE3C_OK)return r;if(raw[0]>=b->joint_count||raw[1]>=b->joint_count||raw[0]==raw[1])return RE3C_ERR_BAD_FORMAT;for(k=0u;k<3u;k++){re3c_u32 v=raw[2u+k*2u];if(raw[3u+k*2u]>0x80u)return RE3C_ERR_BAD_FORMAT;if(!have||v>maxv)maxv=v;have=1;}stream+=8u;}return RE3C_ERR_BAD_FORMAT;}
static int parse_seam(re3c_model*m,const re3c_section*s,re3c_bind_bank*b,re3c_u32 li,re3c_u16 vc){re3c_bind_limb*l;re3c_i16 sj0[256],sj1[256],sw[256];re3c_u32 stream,firstrec,unique=0u,overlap=0u,i;int r;l=&b->limbs[li];l->vertex_bind_valid=0u;l->seam_first_record=b->seam_record_count;l->seam_record_count=0u;l->seam_unique_vertex_count=0u;l->seam_overlap_owner_count=0u;if(!l->vertex_bind_present)return RE3C_OK;for(i=0;i<256u;i++)sj0[i]=sj1[i]=sw[i]=-1;stream=l->seam_stream;firstrec=b->seam_record_count;while(stream<s->size){re3c_u8 first,raw[8];re3c_seam_record rec;re3c_u32 k;r=src_u8(&m->source,s->offset+stream,&first);if(r!=RE3C_OK)return r;if(first==0xFFu){l->seam_record_count=(re3c_u16)((re3c_u32)b->seam_record_count-firstrec);l->seam_unique_vertex_count=(re3c_u16)unique;l->seam_overlap_owner_count=(re3c_u16)overlap;b->seam_unique_vertex_count=(re3c_u16)(b->seam_unique_vertex_count+unique);l->vertex_bind_valid=1u;return RE3C_OK;}if(!insec(s,stream,8u)||b->seam_record_count>=RE3C_MAX_SEAM_RECORDS)return RE3C_ERR_OVERFLOW;r=src_read(&m->source,s->offset+stream,raw,8u);if(r!=RE3C_OK)return r;rec.joint0=raw[0];rec.joint1=raw[1];if(rec.joint0>=b->joint_count||rec.joint1>=b->joint_count||rec.joint0==rec.joint1)return RE3C_ERR_BAD_FORMAT;for(k=0;k<3u;k++){re3c_u8 v=raw[2u+k*2u],w=raw[3u+k*2u];if(v>=vc||w>0x80u)return RE3C_ERR_BAD_FORMAT;rec.vertex[k]=v;rec.blend_q7[k]=w;if(sj0[v]<0){sj0[v]=rec.joint0;sj1[v]=rec.joint1;sw[v]=w;unique++;if(l->joint_a[v]>=0||l->joint_b[v]>=0)overlap++;}else if(sj0[v]!=rec.joint0||sj1[v]!=rec.joint1||sw[v]!=w)return RE3C_ERR_BAD_FORMAT;}b->seam_records[b->seam_record_count++]=rec;stream+=8u;}return RE3C_ERR_BAD_FORMAT;}
static int bind_header(re3c_model*m,const re3c_section*s,re3c_u16 joints,re3c_bind_bank*b){re3c_u8 h[8];re3c_u16 pbind,limbs;re3c_u32 i;int r;if(!s->present||s->size<=4u||!joints)return RE3C_ERR_NOT_PRESENT;bind_clear(b);r=src_read(&m->source,s->offset,h,8u);if(r!=RE3C_OK)return r;pbind=rd16(h+4);limbs=rd16(h+6);if(!limbs)return RE3C_ERR_NOT_PRESENT;if(limbs>RE3C_MAX_BIND_LIMBS||!insec(s,8u,(re3c_u32)limbs*8u)||!insec(s,pbind,(re3c_u32)joints*6u))return RE3C_ERR_BAD_FORMAT;b->joint_count=joints;b->limb_count=limbs;b->bind_pose_offset=pbind;for(i=0u;i<joints;i++){re3c_i16 x,y,z;r=src_i16(&m->source,s->offset+pbind+i*6u,&x);if(r!=RE3C_OK)return r;r=src_i16(&m->source,s->offset+pbind+i*6u+2u,&y);if(r!=RE3C_OK)return r;r=src_i16(&m->source,s->offset+pbind+i*6u+4u,&z);if(r!=RE3C_OK)return r;b->bind_pose[i]=posfx(x,y,z);}return RE3C_OK;}
static re3c_i32 bind_cost(const re3c_bind_limb*l,re3c_u16 vc,re3c_u32 oi){re3c_u32 i,in=0u,out=0u,missing,cost;for(i=0;i<256u;i++)if(l->joint_a[i]>=0){if(i<vc)in++;else out++;}missing=vc>in?(re3c_u32)vc-in:0u;cost=out*10000u+missing*50u+oi;if(l->seam_required_vertex_count>vc)cost+=((re3c_u32)l->seam_required_vertex_count-vc)*20000u;if(cost>0x3FFFFFFFu)cost=0x3FFFFFFFu;return (re3c_i32)cost;}
static void bind_count(const re3c_bind_limb*l,re3c_u16 vc,int lb,re3c_u16*in,re3c_u16*out){const re3c_i16*o=lb?l->joint_b:l->joint_a;re3c_u32 i,a=0u,b=0u;for(i=0;i<256u;i++)if(o[i]>=0){if(i<vc)a++;else b++;}*in=(re3c_u16)a;*out=(re3c_u16)b;}
static int bind_map(re3c_bind_bank*b,const re3c_mesh*m){re3c_i32 prev[RE3C_MAX_PARTS],cur[RE3C_MAX_PARTS],inf=0x3FFFFFFF;re3c_i16 pred[RE3C_MAX_BIND_LIMBS][RE3C_MAX_PARTS],chosen[RE3C_MAX_BIND_LIMBS];re3c_u32 li,oi,pi;if(!b||!m)return RE3C_ERR_BAD_ARGUMENT;if(!b->present||!b->limb_count||!m->part_count)return RE3C_ERR_NOT_PRESENT;if(b->limb_count>m->part_count)return RE3C_ERR_BAD_FORMAT;for(oi=0;oi<RE3C_MAX_PARTS;oi++){prev[oi]=inf;cur[oi]=inf;}for(li=0;li<b->limb_count;li++)for(oi=0;oi<m->part_count;oi++)pred[li][oi]=-1;for(oi=0;oi<m->part_count;oi++)prev[oi]=bind_cost(&b->limbs[0],m->parts[oi].vertex_count,oi);for(li=1;li<b->limb_count;li++){for(oi=0;oi<m->part_count;oi++){re3c_i32 bp=inf,c;re3c_i16 bi=-1;for(pi=0;pi<oi;pi++)if(prev[pi]<bp){bp=prev[pi];bi=(re3c_i16)pi;}cur[oi]=inf;if(bi<0||bp>=inf)continue;c=bind_cost(&b->limbs[li],m->parts[oi].vertex_count,oi);if(c<inf-bp){cur[oi]=bp+c;pred[li][oi]=bi;}}for(oi=0;oi<m->part_count;oi++)prev[oi]=cur[oi];} {re3c_i32 bc=inf;re3c_i16 bo=-1;for(oi=0;oi<m->part_count;oi++)if(prev[oi]<bc){bc=prev[oi];bo=(re3c_i16)oi;}if(bo<0)return RE3C_ERR_BAD_FORMAT;chosen[b->limb_count-1u]=bo;}li=b->limb_count-1u;while(li>0u){re3c_i16 q=pred[li][(re3c_u32)chosen[li]];if(q<0)return RE3C_ERR_BAD_FORMAT;chosen[li-1u]=q;li--;}
 b->mapped_limb_count=0u;b->owner_outlier_count=0u;b->owner_a_vertex_count=b->owner_b_vertex_count=0u;for(li=0;li<b->limb_count;li++){re3c_bind_limb*l=&b->limbs[li];re3c_u16 ia,oa,ib,ob,vc;l->mapped_part=chosen[li];vc=m->parts[(re3c_u32)chosen[li]].vertex_count;bind_count(l,vc,0,&ia,&oa);bind_count(l,vc,1,&ib,&ob);l->owner_a_out_of_range=oa;l->owner_b_out_of_range=ob;l->map_status=(re3c_u8)((oa==0u&&ia==vc)?RE3C_BIND_MAP_EXACT:RE3C_BIND_MAP_INFERRED);b->owner_a_vertex_count+=ia;b->owner_b_vertex_count+=ib;b->owner_outlier_count=(re3c_u16)(b->owner_outlier_count+oa+ob);b->mapped_limb_count++;}b->mesh_map_resolved=1u;b->mesh_map_complete=(re3c_u8)(b->mapped_limb_count==b->limb_count);return RE3C_OK;}
static int parse_bind_chunk(re3c_model*m,const re3c_section*s,re3c_u16 joints,const re3c_mesh*mesh,re3c_bind_bank*b){re3c_u32 i;int r;r=bind_header(m,s,joints,b);if(r!=RE3C_OK)return r;b->parse_mode=RE3C_BIND_PARSE_OWNERSHIP;b->seam_record_count=0u;b->seam_unique_vertex_count=0u;for(i=0;i<b->limb_count;i++){re3c_u8 idx[8];re3c_bind_limb*l=&b->limbs[i];re3c_u16 reserved;re3c_u32 base,seam;r=src_read(&m->source,s->offset+8u+i*8u,idx,8u);if(r!=RE3C_OK)return r;l->tri_a_offset=rd16(idx);l->tri_b_offset=rd16(idx+2);l->vertex_bind_offset=rd16(idx+4);reserved=rd16(idx+6);if(reserved)b->partial=1u;r=parse_owner_stream(m,s,i,l->tri_a_offset,joints,l->joint_a,&l->tri_a_present,&l->owner_a_stream,&l->owner_a_count);if(r!=RE3C_OK)return r;if(!l->tri_a_present)return RE3C_ERR_BAD_FORMAT;r=parse_owner_stream(m,s,i,l->tri_b_offset,joints,l->joint_b,&l->tri_b_present,&l->owner_b_stream,&l->owner_b_count);if(r!=RE3C_OK)return r;l->vertex_bind_present=0u;if(l->vertex_bind_offset){base=8u+i*8u;seam=base+l->vertex_bind_offset;if(seam>=s->size||seam>65535u)return RE3C_ERR_BAD_FORMAT;l->vertex_bind_present=1u;l->seam_stream=(re3c_u16)seam;r=scan_seam_req(m,s,b,i);if(r!=RE3C_OK)return r;}b->decoded_limb_count++;}b->present=1u;r=bind_map(b,mesh);if(r!=RE3C_OK){b->partial=1u;return RE3C_OK;}for(i=0;i<b->limb_count;i++){re3c_bind_limb*l=&b->limbs[i];if(l->mapped_part<0||(re3c_u32)l->mapped_part>=mesh->part_count)return RE3C_ERR_BAD_FORMAT;r=parse_seam(m,s,b,i,mesh->parts[(re3c_u32)l->mapped_part].vertex_count);if(r!=RE3C_OK)return r;}return RE3C_OK;}
int re3c_validate_inverse_bind(const re3c_anim_bank*bank,const re3c_bind_bank*bind){re3c_u32 i;re3c_vec3fx root;if(!bank||!bind)return RE3C_ERR_BAD_ARGUMENT;if(!bind->present||!bank->present||bind->joint_count!=bank->joint_count)return RE3C_ERR_NOT_PRESENT;if(!bank->joint_count)return RE3C_ERR_NOT_PRESENT;root=vadd(bank->joints[0].world_pos,bind->bind_pose[0]);for(i=1u;i<bank->joint_count;i++){re3c_vec3fx q=vadd(bank->joints[i].world_pos,bind->bind_pose[i]);if(q.x!=root.x||q.y!=root.y||q.z!=root.z)return RE3C_ERR_BAD_FORMAT;}return RE3C_OK;}
int re3c_parse_bind_banks(re3c_model*m){int a,b;if(!m)return RE3C_ERR_BAD_ARGUMENT;bind_clear(&m->bind_a);bind_clear(&m->bind_b);a=parse_bind_chunk(m,&m->sections[RE3C_SEC_BINARY00],m->banks[RE3C_BANK_A].joint_count,&m->body,&m->bind_a);if(a==RE3C_OK){m->bind_a.inverse_bind_checked=1u;if(re3c_validate_inverse_bind(&m->banks[RE3C_BANK_A],&m->bind_a)==RE3C_OK){m->bind_a.inverse_bind_valid=1u;m->bind_a.bind_root=vadd(m->banks[RE3C_BANK_A].joints[0].world_pos,m->bind_a.bind_pose[0]);}}else if(a!=RE3C_ERR_NOT_PRESENT)return a;b=RE3C_ERR_NOT_PRESENT;if(m->sections[RE3C_SEC_BINARY01].size<=4u)b=parse_bind_chunk(m,&m->sections[RE3C_SEC_BINARY01],m->banks[RE3C_BANK_B].joint_count,&m->body,&m->bind_b);if(b==RE3C_OK){m->bind_b.inverse_bind_checked=1u;if(re3c_validate_inverse_bind(&m->banks[RE3C_BANK_B],&m->bind_b)==RE3C_OK){m->bind_b.inverse_bind_valid=1u;m->bind_b.bind_root=vadd(m->banks[RE3C_BANK_B].joints[0].world_pos,m->bind_b.bind_pose[0]);}}else if(b!=RE3C_ERR_NOT_PRESENT)return b;return RE3C_OK;}
int re3c_bind_limb_for_part(const re3c_bind_bank*b,re3c_u32 part,re3c_u32*out){re3c_u32 i;if(!b||!out)return RE3C_ERR_BAD_ARGUMENT;if(!b->present||!b->mesh_map_resolved)return RE3C_ERR_NOT_PRESENT;for(i=0;i<b->limb_count;i++)if(b->limbs[i].mapped_part>=0&&(re3c_u32)b->limbs[i].mapped_part==part){*out=i;return RE3C_OK;}return RE3C_ERR_NOT_PRESENT;}
int re3c_bind_joint_for_vertex(const re3c_bind_bank*b,re3c_u32 part,re3c_u32 v,int list_b,re3c_i16*out){re3c_u32 li;const re3c_i16*o;int r;if(!out)return RE3C_ERR_BAD_ARGUMENT;r=re3c_bind_limb_for_part(b,part,&li);if(r!=RE3C_OK)return r;if(v>=256u)return RE3C_ERR_RANGE;o=list_b?b->limbs[li].joint_b:b->limbs[li].joint_a;if(o[v]<0)return RE3C_ERR_NOT_PRESENT;*out=o[v];return RE3C_OK;}
int re3c_bind_seam_for_vertex(const re3c_bind_bank*b,re3c_u32 part,re3c_u32 v,re3c_u8*j0,re3c_u8*j1,re3c_u8*w){re3c_u32 li,i;const re3c_bind_limb*l;int r;if(!b||!j0||!j1||!w)return RE3C_ERR_BAD_ARGUMENT;r=re3c_bind_limb_for_part(b,part,&li);if(r!=RE3C_OK)return r;l=&b->limbs[li];if(!l->vertex_bind_present||!l->vertex_bind_valid)return RE3C_ERR_NOT_PRESENT;for(i=0;i<l->seam_record_count;i++){const re3c_seam_record*rec=&b->seam_records[(re3c_u32)l->seam_first_record+i];re3c_u32 k;for(k=0;k<3u;k++)if(rec->vertex[k]==v){*j0=rec->joint0;*j1=rec->joint1;*w=rec->blend_q7[k];return RE3C_OK;}}return RE3C_ERR_NOT_PRESENT;}
int re3c_bind_blend_q7_to_fx(re3c_u8 b,re3c_fx*w0,re3c_fx*w1){if(!w0||!w1)return RE3C_ERR_BAD_ARGUMENT;if(b>0x80u)return RE3C_ERR_RANGE;*w1=(re3c_fx)((re3c_u32)b<<5);*w0=RE3C_FX_ONE-*w1;return RE3C_OK;}
int re3c_bind_blend_positions_q7(const re3c_vec3fx*a,const re3c_vec3fx*b,re3c_u8 w,re3c_vec3fx*out){re3c_fx w0,w1;int r;if(!a||!b||!out)return RE3C_ERR_BAD_ARGUMENT;r=re3c_bind_blend_q7_to_fx(w,&w0,&w1);if(r!=RE3C_OK)return r;out->x=re3c_fx_mul(a->x,w0)+re3c_fx_mul(b->x,w1);out->y=re3c_fx_mul(a->y,w0)+re3c_fx_mul(b->y,w1);out->z=re3c_fx_mul(a->z,w0)+re3c_fx_mul(b->z,w1);return RE3C_OK;}
int re3c_bind_vertex_to_joint_local(const re3c_bind_bank*b,re3c_u32 j,const re3c_vec3fx*v,re3c_vec3fx*out){if(!b||!v||!out)return RE3C_ERR_BAD_ARGUMENT;if(!b->present||j>=b->joint_count)return RE3C_ERR_RANGE;*out=vadd(*v,b->bind_pose[j]);return RE3C_OK;}
int re3c_bind_vertex_reference_pose(const re3c_anim_bank*bank,const re3c_bind_bank*b,re3c_u32 j,const re3c_vec3fx*v,re3c_vec3fx*out){re3c_vec3fx local;if(!bank||!b||!v||!out)return RE3C_ERR_BAD_ARGUMENT;if(j>=bank->joint_count)return RE3C_ERR_RANGE;if(re3c_bind_vertex_to_joint_local(b,j,v,&local)!=RE3C_OK)return RE3C_ERR_RANGE;*out=vadd(local,bank->joints[j].world_pos);return RE3C_OK;}
int re3c_skin_vertex_joint(const re3c_bind_bank*b,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 j,const re3c_vec3fx*v,re3c_vec3fx*out){re3c_vec3fx local;int r;if(!b||!mats||!v||!out)return RE3C_ERR_BAD_ARGUMENT;r=re3c_bind_vertex_to_joint_local(b,j,v,&local);if(r!=RE3C_OK)return r;return re3c_mat34_transform_point(&mats[j],&local,out);}
int re3c_skin_vertex_seam(const re3c_bind_bank*b,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u8 j0,re3c_u8 j1,re3c_u8 w,const re3c_vec3fx*v,re3c_vec3fx*out){re3c_vec3fx a,c;int r;if(!b||!mats||!v||!out)return RE3C_ERR_BAD_ARGUMENT;r=re3c_skin_vertex_joint(b,mats,j0,v,&a);if(r!=RE3C_OK)return r;r=re3c_skin_vertex_joint(b,mats,j1,v,&c);if(r!=RE3C_OK)return r;return re3c_bind_blend_positions_q7(&a,&c,w,out);}

int re3c_classify_body_parts(re3c_model*m){re3c_u32 i;if(!m)return RE3C_ERR_BAD_ARGUMENT;for(i=0;i<m->body.part_count;i++){re3c_mesh_part*p=&m->body.parts[i];re3c_u32 li,mt;p->part_class=RE3C_PART_UNKNOWN;p->part_flags=0u;p->driver_joint=-1;p->bind_limb=-1;p->morph_track=-1;p->morph_frame_count=0u;if(re3c_bind_limb_for_part(&m->bind_a,i,&li)==RE3C_OK){p->part_class=RE3C_PART_DAT_SKINNED;p->part_flags=RE3C_PART_FLAG_RENDER_DEFAULT|RE3C_PART_FLAG_DAT;p->bind_limb=(re3c_i16)li;}else if(p->vertex_count==3u&&p->face_count==1u){p->part_class=RE3C_PART_JOINT_HELPER;p->part_flags=RE3C_PART_FLAG_HELPER;if(i<m->banks[RE3C_BANK_A].joint_count)p->driver_joint=(re3c_i16)i;}else if(i<m->banks[RE3C_BANK_A].joint_count){p->part_class=RE3C_PART_RIGID_JOINT;p->part_flags=RE3C_PART_FLAG_RENDER_DEFAULT;p->driver_joint=(re3c_i16)i;}else{p->part_class=RE3C_PART_EXTRA_UNBOUND;p->part_flags=RE3C_PART_FLAG_RENDER_DEFAULT;}if(re3c_morph_track_for_part(m,i,&mt)==RE3C_OK){p->morph_track=(re3c_i16)mt;p->morph_frame_count=m->binary01_morph.tracks[mt].frame_count;p->part_flags|=RE3C_PART_FLAG_BINARY01_MORPH;if(p->part_class==RE3C_PART_EXTRA_UNBOUND)p->part_class=RE3C_PART_DYNAMIC_MORPH;}}return RE3C_OK;}
/* Reference pose: DAT owner/seam maps and rigid joint translations. */
int re3c_reference_body_vertex(const re3c_model*m,re3c_u32 part,re3c_u32 local_vertex,re3c_vec3fx*out){
 const re3c_mesh_part*p;const re3c_vec3fx*v;const re3c_anim_bank*b;
 re3c_i16 j;re3c_u8 j0,j1,w;re3c_vec3fx a,c;int r;
 if(!m||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(part>=m->body.part_count)return RE3C_ERR_RANGE;
 p=&m->body.parts[part];if(local_vertex>=p->vertex_count)return RE3C_ERR_RANGE;
 v=&m->body.vertices[p->first_vertex+local_vertex].pos;b=&m->banks[RE3C_BANK_A];
 if(p->part_class==RE3C_PART_DAT_SKINNED&&b->present&&m->bind_a.present){
  r=re3c_bind_seam_for_vertex(&m->bind_a,part,local_vertex,&j0,&j1,&w);
  if(r==RE3C_OK&&j0<b->joint_count&&j1<b->joint_count){
   r=re3c_bind_vertex_reference_pose(b,&m->bind_a,j0,v,&a);
   if(r==RE3C_OK){r=re3c_bind_vertex_reference_pose(b,&m->bind_a,j1,v,&c);
    if(r==RE3C_OK)return re3c_bind_blend_positions_q7(&a,&c,w,out);}}
  r=re3c_bind_joint_for_vertex(&m->bind_a,part,local_vertex,0,&j);
  if(r!=RE3C_OK)r=re3c_bind_joint_for_vertex(&m->bind_a,part,local_vertex,1,&j);
  if(r==RE3C_OK&&j>=0&&(re3c_u32)j<b->joint_count)
   return re3c_bind_vertex_reference_pose(b,&m->bind_a,(re3c_u32)j,v,out);
 }
 *out=*v;
 if(b->present&&b->joint_count){
  re3c_u32 idx=0u;
  if(p->driver_joint>=0&&(re3c_u32)p->driver_joint<b->joint_count)idx=(re3c_u32)p->driver_joint;
  else if(p->part_class==RE3C_PART_RIGID_JOINT&&part<b->joint_count)idx=part;
  out->x+=b->joints[idx].world_pos.x;
  out->y+=b->joints[idx].world_pos.y;
  out->z+=b->joints[idx].world_pos.z;
 }
 return RE3C_OK;
}
const char*re3c_part_class_name(re3c_u32 c){switch(c){case RE3C_PART_DAT_SKINNED:return"dat_skinned";case RE3C_PART_RIGID_JOINT:return"rigid_joint";case RE3C_PART_JOINT_HELPER:return"joint_helper";case RE3C_PART_EXTRA_UNBOUND:return"extra_unbound";case RE3C_PART_DYNAMIC_MORPH:return"dynamic_morph";default:return"unknown";}}
int re3c_part_should_render_default(const re3c_mesh_part*p){if(!p)return 0;return (p->part_flags&RE3C_PART_FLAG_RENDER_DEFAULT)?1:0;}

static int finish_open(re3c_model*m){int r,aux;r=parse_dir(m);if(r!=RE3C_OK)return r;m->is_open=1u;r=re3c_parse_all_banks(m);if(r!=RE3C_OK)return r;r=re3c_parse_body_mesh(m);if(r!=RE3C_OK)return r;aux=re3c_parse_aux_mesh(m);if(aux!=RE3C_OK&&aux!=RE3C_ERR_NOT_PRESENT)return aux;r=re3c_parse_binary01_morph(m);if(r!=RE3C_OK)return r;r=re3c_parse_bind_banks(m);if(r!=RE3C_OK)return r;r=re3c_classify_body_parts(m);if(r!=RE3C_OK)return r;return RE3C_OK;}
int re3c_open_memory(re3c_model*m,const void*d,size_t n){int r;if(!m||!d)return RE3C_ERR_BAD_ARGUMENT;re3c_model_close(m);m->source.memory=(const re3c_u8*)d;m->source.size=n;m->source.owns_file=0;r=finish_open(m);if(r!=RE3C_OK){re3c_model_close(m);return r;}return RE3C_OK;}
int re3c_open_file(re3c_model*m,const char*p){FILE*f;long e;int r;if(!m||!p)return RE3C_ERR_BAD_ARGUMENT;re3c_model_close(m);f=fopen(p,"rb");if(!f)return RE3C_ERR_IO;if(fseek(f,0L,SEEK_END)!=0){fclose(f);return RE3C_ERR_IO;}e=ftell(f);if(e<8L||(long)(re3c_u32)e!=e){fclose(f);return RE3C_ERR_OVERFLOW;}if(fseek(f,0L,SEEK_SET)!=0){fclose(f);return RE3C_ERR_IO;}m->source.fp=f;m->source.size=(size_t)e;m->source.owns_file=1;r=finish_open(m);if(r!=RE3C_OK){re3c_model_close(m);return r;}return RE3C_OK;}
int re3c_validate(const re3c_model*m,char*report,size_t cap){re3c_u32 i;size_t used=0u;if(!m)return RE3C_ERR_BAD_ARGUMENT;if(report&&cap)report[0]='\0';if(!m->is_open)return RE3C_ERR_NOT_OPEN;if(m->section_count!=15u||!m->sections[RE3C_SEC_MD2_BODY].present)return RE3C_ERR_BAD_FORMAT;if(!m->body.part_count||!m->body.vertex_count)return RE3C_ERR_BAD_FORMAT;for(i=0;i<m->body.part_count;i++)if(m->body.parts[i].part_class==RE3C_PART_UNKNOWN)return RE3C_ERR_BAD_FORMAT;if(report&&cap){const char*ok="RE3 codec structural validation: OK\n";size_t n=strlen(ok);if(n>=cap)n=cap-1u;memcpy(report,ok,n);report[n]='\0';used=n;(void)used;}return RE3C_OK;}

/* v0.18: explicit, testable morph-state vertex selection.  The original
 * animation transform remains identical to v0.17 when morph_frame==REST. */
static int re3c_animated_body_vertex_source(const re3c_model*m,re3c_u32 bi,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 local_vertex,const re3c_vec3fx*v,re3c_vec3fx*out){
 const re3c_mesh_part*p;const re3c_anim_bank*b;re3c_i16 j;re3c_u8 j0,j1,w;int r;
 if(!m||!mats||!out||!v)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(bi>=RE3C_BANK_COUNT||part>=m->body.part_count)return RE3C_ERR_RANGE;
 b=&m->banks[bi];if(!b->present||!b->joint_count)return RE3C_ERR_NOT_PRESENT;
 if(!m->banks[RE3C_BANK_A].present||b->joint_count!=m->banks[RE3C_BANK_A].joint_count)return RE3C_ERR_NOT_PRESENT;
 p=&m->body.parts[part];if(local_vertex>=p->vertex_count)return RE3C_ERR_RANGE;
 if(p->part_class==RE3C_PART_DAT_SKINNED&&m->bind_a.present){
  if(m->bind_a.joint_count!=b->joint_count)return RE3C_ERR_NOT_PRESENT;
  r=re3c_bind_seam_for_vertex(&m->bind_a,part,local_vertex,&j0,&j1,&w);
  if(r==RE3C_OK&&j0<b->joint_count&&j1<b->joint_count)
   return re3c_skin_vertex_seam(&m->bind_a,mats,j0,j1,w,v,out);
  r=re3c_bind_joint_for_vertex(&m->bind_a,part,local_vertex,0,&j);
  if(r!=RE3C_OK)r=re3c_bind_joint_for_vertex(&m->bind_a,part,local_vertex,1,&j);
  if(r==RE3C_OK&&j>=0&&(re3c_u32)j<b->joint_count)
   return re3c_skin_vertex_joint(&m->bind_a,mats,(re3c_u32)j,v,out);
 }
 {
  re3c_u32 idx=0u;
  if(p->driver_joint>=0&&(re3c_u32)p->driver_joint<b->joint_count)idx=(re3c_u32)p->driver_joint;
  else if(p->part_class==RE3C_PART_RIGID_JOINT&&part<b->joint_count)idx=part;
  return re3c_mat34_transform_point(&mats[idx],v,out);
 }
}
int re3c_animated_body_vertex(const re3c_model*m,re3c_u32 bi,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 local_vertex,re3c_vec3fx*out){
 const re3c_mesh_part*p;
 if(!m||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(part>=m->body.part_count)return RE3C_ERR_RANGE;
 p=&m->body.parts[part];if(local_vertex>=p->vertex_count)return RE3C_ERR_RANGE;
 return re3c_animated_body_vertex_source(m,bi,mats,part,local_vertex,&m->body.vertices[p->first_vertex+local_vertex].pos,out);
}
int re3c_animated_body_vertex_morph(const re3c_model*m,re3c_u32 bi,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 local_vertex,re3c_u32 morph_frame,re3c_vec3fx*out){
 re3c_vec3fx v;re3c_u32 track;int r;
 if(!m||!mats||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(part>=m->body.part_count)return RE3C_ERR_RANGE;
 if(local_vertex>=m->body.parts[part].vertex_count)return RE3C_ERR_RANGE;
 if(morph_frame==RE3C_MORPH_REST)return re3c_animated_body_vertex(m,bi,mats,part,local_vertex,out);
 r=re3c_morph_track_for_part(m,part,&track);
 if(r==RE3C_ERR_NOT_PRESENT)return re3c_animated_body_vertex(m,bi,mats,part,local_vertex,out);
 if(r!=RE3C_OK)return r;
 if(morph_frame>=m->binary01_morph.tracks[track].frame_count)return RE3C_ERR_RANGE;
 r=re3c_morph_vertex(m,part,morph_frame,local_vertex,&v);
 if(r!=RE3C_OK)return r;
 return re3c_animated_body_vertex_source(m,bi,mats,part,local_vertex,&v,out);
}
/* Explicit diagnostic centroid re-centering for binary01 morph frames.
 * C89: sum raw 16-bit integer units (mesh parts <= 256 vertices).
 * No automatic inference of Capcom's morph coordinate space is made here. */
int re3c_morph_center_translation(const re3c_model*m,re3c_u32 part,re3c_u32 frame,re3c_vec3fx*out){
 re3c_u32 ti,k,n;re3c_i32 ox=0,oy=0,oz=0,mx=0,my=0,mz=0;int r;
 if(!m||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(part>=m->body.part_count)return RE3C_ERR_RANGE;
 if(frame==RE3C_MORPH_REST){out->x=out->y=out->z=0;return RE3C_OK;}
 r=re3c_morph_track_for_part(m,part,&ti);if(r!=RE3C_OK)return r;
 if(frame>=m->binary01_morph.tracks[ti].frame_count)return RE3C_ERR_RANGE;
 n=m->body.parts[part].vertex_count;if(!n||n>256u)return RE3C_ERR_RANGE;
 for(k=0u;k<n;k++){
  re3c_vec3fx v;const re3c_vec3fx*orig=&m->body.vertices[m->body.parts[part].first_vertex+k].pos;
  r=re3c_morph_vertex(m,part,frame,k,&v);if(r!=RE3C_OK)return r;
  ox+=re3c_fx_to_int(orig->x);oy+=re3c_fx_to_int(orig->y);oz+=re3c_fx_to_int(orig->z);
  mx+=re3c_fx_to_int(v.x);my+=re3c_fx_to_int(v.y);mz+=re3c_fx_to_int(v.z);
 }
 out->x=((ox-mx)/(re3c_i32)n)*RE3C_FX_ONE;
 out->y=((oy-my)/(re3c_i32)n)*RE3C_FX_ONE;
 out->z=((oz-mz)/(re3c_i32)n)*RE3C_FX_ONE;
 return RE3C_OK;
}
int re3c_animated_body_vertex_morph_offset(const re3c_model*m,re3c_u32 bi,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 vertex,re3c_u32 frame,const re3c_vec3fx*translation,re3c_vec3fx*out){
 re3c_vec3fx v;int r;
 if(!m||!mats||!translation||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(part>=m->body.part_count||vertex>=m->body.parts[part].vertex_count)return RE3C_ERR_RANGE;
 if(frame==RE3C_MORPH_REST)return re3c_animated_body_vertex(m,bi,mats,part,vertex,out);
 r=re3c_morph_vertex(m,part,frame,vertex,&v);if(r!=RE3C_OK)return r;
 /* Local translation is applied BEFORE bind/skin/rotation. */
 v.x+=translation->x;v.y+=translation->y;v.z+=translation->z;
 return re3c_animated_body_vertex_source(m,bi,mats,part,vertex,&v,out);
}
int re3c_animated_body_vertex_morph_delta(const re3c_model*m,re3c_u32 bi,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 vertex,re3c_u32 frame,re3c_vec3fx*out){
 re3c_vec3fx v;const re3c_vec3fx *base;int r;
 if(!m||!mats||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(part>=m->body.part_count||vertex>=m->body.parts[part].vertex_count)return RE3C_ERR_RANGE;
 if(frame==RE3C_MORPH_REST||frame==0u)return re3c_animated_body_vertex_morph(m,bi,mats,part,vertex,frame,out);
 r=re3c_morph_vertex(m,part,frame,vertex,&v);if(r!=RE3C_OK)return r;
 base=&m->body.vertices[m->body.parts[part].first_vertex+vertex].pos;
 v.x+=base->x;v.y+=base->y;v.z+=base->z;
 return re3c_animated_body_vertex_source(m,bi,mats,part,vertex,&v,out);
}
int re3c_retarget_pose_explicit(const re3c_pose*source,const re3c_anim_bank*target,const re3c_u16 map[],re3c_pose*out){
 re3c_u32 i;re3c_pose tmp;
 if(!source||!target||!map||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(!target->present||!target->joint_count||!source->joint_count)return RE3C_ERR_NOT_PRESENT;
 if(target->joint_count>RE3C_MAX_PARTS||source->joint_count>RE3C_MAX_PARTS)return RE3C_ERR_RANGE;
 if(target->hierarchy_source==255u)return RE3C_ERR_NOT_PRESENT;
 for(i=0;i<target->joint_count;i++)
  if(map[i]!=RE3C_JOINT_NEUTRAL&&(re3c_u32)map[i]>=source->joint_count)return RE3C_ERR_RANGE;
 memset(&tmp,0,sizeof(tmp));tmp.joint_count=target->joint_count;
 tmp.speed=source->speed;tmp.origin_y=source->origin_y;
 for(i=0;i<target->joint_count;i++)
  if(map[i]!=RE3C_JOINT_NEUTRAL){
   re3c_u32 j=map[i];tmp.rot_x[i]=source->rot_x[j];tmp.rot_y[i]=source->rot_y[j];tmp.rot_z[i]=source->rot_z[j];
  }
 *out=tmp;return RE3C_OK;
}

/* v0.19: deterministic per-part morph + visibility state, controlled externally.
 * Never assume EMD animation keys alone imply script triggers. */
int re3c_visual_state_init(const re3c_model *m,re3c_visual_state *state,int all){
 re3c_u32 p;
 if(!m||!state)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 memset(state,0,sizeof(*state));
 for(p=0u;p<m->body.part_count;p++){
  const re3c_mesh_part *part=&m->body.parts[p];
  int vis=re3c_part_should_render_default(part);
  if(!all&&(part->part_class==RE3C_PART_EXTRA_UNBOUND||part->part_class==RE3C_PART_DYNAMIC_MORPH))vis=0;
  state->visible[p]=(re3c_u8)(vis?1u:0u);
  state->morph_frame[p]=RE3C_MORPH_REST;
 }
 return RE3C_OK;
}
int re3c_visual_state_set_visibility(const re3c_model *m,re3c_visual_state *state,re3c_u32 part,int visible){
 if(!m||!state)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(part>=m->body.part_count)return RE3C_ERR_RANGE;
 if(visible!=0&&visible!=1)return RE3C_ERR_BAD_ARGUMENT;
 state->visible[part]=(re3c_u8)visible;
 return RE3C_OK;
}
int re3c_visual_state_set_morph(const re3c_model *m,re3c_visual_state *state,re3c_u32 part,re3c_u32 frame){
 re3c_u32 ti; int r;
 if(!m||!state)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(part>=m->body.part_count)return RE3C_ERR_RANGE;
 if(frame!=RE3C_MORPH_REST){
  r=re3c_morph_track_for_part(m,part,&ti);
  if(r!=RE3C_OK)return r;
  if(frame>=m->binary01_morph.tracks[ti].frame_count)return RE3C_ERR_RANGE;
 }
 state->morph_frame[part]=frame;
 return RE3C_OK;
}
int re3c_animated_body_vertex_state(const re3c_model *m,re3c_u32 bi,const re3c_mat34fx mats[RE3C_MAX_PARTS],const re3c_visual_state *state,re3c_u32 part,re3c_u32 v,re3c_vec3fx *out){
 if(!m||!mats||!state||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(part>=m->body.part_count)return RE3C_ERR_RANGE;
 return re3c_animated_body_vertex_morph(m,bi,mats,part,v,state->morph_frame[part],out);
}

/* Ghidra-confirmed RE3 PC polygon atlas remapping. Hardware-equivalent
 * integer word additions, preserving carries exactly as the x86 code.
 * No floats, heap, or external dependencies. */
int re3c_pc_rebase_polygon_record(re3c_u8 *record,size_t bytes,re3c_u32 page_x,re3c_u32 clut_y){
 re3c_u32 w0,w1;
 if(!record)return RE3C_ERR_BAD_ARGUMENT;
 if(bytes!=12u&&bytes!=16u)return RE3C_ERR_BAD_ARGUMENT;
 if(page_x>255u||clut_y>255u)return RE3C_ERR_RANGE;
 w0=(re3c_u32)record[0]|((re3c_u32)record[1]<<8)|((re3c_u32)record[2]<<16)|((re3c_u32)record[3]<<24);
 w1=(re3c_u32)record[4]|((re3c_u32)record[5]<<8)|((re3c_u32)record[6]<<16)|((re3c_u32)record[7]<<24);
 w0+=(re3c_u32)(clut_y<<22);
 w1+=(re3c_u32)(page_x<<16);
 record[0]=(re3c_u8)w0;record[1]=(re3c_u8)(w0>>8);record[2]=(re3c_u8)(w0>>16);record[3]=(re3c_u8)(w0>>24);
 record[4]=(re3c_u8)w1;record[5]=(re3c_u8)(w1>>8);record[6]=(re3c_u8)(w1>>16);record[7]=(re3c_u8)(w1>>24);
 return RE3C_OK;
}

/* Ghidra recovery: RE3 PC morph reconstruction from 0x00438200 and
 * 0x00438260. Local xyz are stored as signed 16-bit integers in binary01;
 * each subsequent morph frame contributes delta * weight >> 12.
 * Runtime control fields that set weights/which parts are active have not
 * been reconstructed; callers supply explicit weights here. */
static re3c_i32 re3c_pc_sar12(re3c_i32 x){
 if(x>=0)return x/4096;
 return -( ((-x)/4096) + (((-x)%4096)!=0 ? 1 : 0) );
}
int re3c_morph_blend_local_vertex(const re3c_model*m,re3c_u32 part,re3c_u32 vertex,const re3c_u16*weights,re3c_u32 n,re3c_vec3fx*out){
 re3c_u32 track,k;re3c_vec3fx base,delta;re3c_i32 x,y,z;int r;
 if(!m||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(!m->is_open)return RE3C_ERR_NOT_OPEN;
 if(part>=m->body.part_count)return RE3C_ERR_RANGE;
 if(vertex>=m->body.parts[part].vertex_count)return RE3C_ERR_RANGE;
 r=re3c_morph_track_for_part(m,part,&track);
 if(r==RE3C_ERR_NOT_PRESENT){
  if(n!=0u)return RE3C_ERR_BAD_ARGUMENT;
  *out=m->body.vertices[m->body.parts[part].first_vertex+vertex].pos;
  return RE3C_OK;
 }
 if(r!=RE3C_OK)return r;
 if(m->binary01_morph.tracks[track].frame_count<1u)return RE3C_ERR_BAD_FORMAT;
 if(n!=(re3c_u32)m->binary01_morph.tracks[track].frame_count-1u)return RE3C_ERR_BAD_ARGUMENT;
 if(n>0u&&!weights)return RE3C_ERR_BAD_ARGUMENT;
 for(k=0u;k<n;k++)if(weights[k]>4096u)return RE3C_ERR_RANGE;
 r=re3c_morph_vertex(m,part,0u,vertex,&base);if(r!=RE3C_OK)return r;
 x=base.x/4096;y=base.y/4096;z=base.z/4096;
 for(k=0u;k<n;k++){
  if(weights[k]==0u)continue;
  r=re3c_morph_vertex(m,part,k+1u,vertex,&delta);if(r!=RE3C_OK)return r;
  x+=re3c_pc_sar12((delta.x/4096)*(re3c_i32)weights[k]);
  y+=re3c_pc_sar12((delta.y/4096)*(re3c_i32)weights[k]);
  z+=re3c_pc_sar12((delta.z/4096)*(re3c_i32)weights[k]);
 }
 /* Original runtime writes signed 16-bit coords into the vertex buffer.
  * Real-corpus weights stay within representable range for these tests;
  * reject overflow instead of invoking implementation-defined casts. */
 if(x< -32768||x>32767||y< -32768||y>32767||z< -32768||z>32767)return RE3C_ERR_OVERFLOW;
 out->x=re3c_fx_from_int(x);out->y=re3c_fx_from_int(y);out->z=re3c_fx_from_int(z);
 return RE3C_OK;
}
int re3c_animated_body_vertex_morph_blend(const re3c_model*m,re3c_u32 bi,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 vertex,const re3c_u16*weights,re3c_u32 n,re3c_vec3fx*out){
 re3c_vec3fx local;int r;
 if(!m||!mats||!out)return RE3C_ERR_BAD_ARGUMENT;
 r=re3c_morph_blend_local_vertex(m,part,vertex,weights,n,&local);
 if(r!=RE3C_OK)return r;
 return re3c_animated_body_vertex_source(m,bi,mats,part,vertex,&local,out);
}

/* v0.25: a guarded, opt-in retarget for the 11-joint RE3 zombie rig.
 * The 11-joint rest hierarchy and local coordinates match the reduced
 * skeleton found in EM16/18-1F. Maps to 15-joint shared animation channel
 * indices; not a claim of runtime behavior for arbitrary enemies. */
int re3c_retarget_zombie_damage_15_to_11(const re3c_model *model,
                                          const re3c_pose *source,
                                          re3c_pose *out){
 static const re3c_i16 parents[11]={-1,0,0,2,0,4,0,6,7,6,9};
 static const re3c_i16 local_xyz[11][3]={
  {0,-1995,0},{-43,-646,0},{-46,-648,-343},{13,581,-61},
  {-46,-648,343},{13,572,61},{0,0,0},{0,253,-176},
  {0,730,-1},{0,253,176},{0,730,1}
 };
 static const re3c_u16 map[11]={0u,1u,2u,3u,5u,6u,8u,9u,10u,12u,13u};
 const re3c_anim_bank *target;
 re3c_u32 j;
 if(!model||!source||!out)return RE3C_ERR_BAD_ARGUMENT;
 if(!model->is_open)return RE3C_ERR_NOT_OPEN;
 target=&model->banks[RE3C_BANK_A];
 if(!target->present||target->joint_count!=11u||source->joint_count!=15u)
  return RE3C_ERR_NOT_PRESENT;
 if(target->hierarchy_source==255u)return RE3C_ERR_NOT_PRESENT;
 for(j=0u;j<11u;j++){
  const re3c_joint *a=&target->joints[j];
  if(a->parent!=parents[j]||
     a->local_pos.x!=re3c_fx_from_int(local_xyz[j][0])||
     a->local_pos.y!=re3c_fx_from_int(local_xyz[j][1])||
     a->local_pos.z!=re3c_fx_from_int(local_xyz[j][2]))
   return RE3C_ERR_NOT_PRESENT;
 }
 return re3c_retarget_pose_explicit(source,target,map,out);
}

/* v0.26: experimental damage rigs. Identity/layout guard relies on exact
 * normal joint parent+rest translations, not on arbitrary model filenames. */
static const re3c_i16 damage_nm_rest[16][4]={
 {-1,0,-2300,0},
 {0,0,-155,0},
 {1,-37,-679,0},
 {1,-36,-410,-480},
 {3,0,435,-37},
 {4,10,500,-19},
 {1,-36,-410,480},
 {6,0,435,37},
 {7,10,500,19},
 {0,0,0,0},
 {9,-29,299,-230},
 {10,-20,779,0},
 {11,-9,899,0},
 {9,-29,299,230},
 {13,-20,779,0},
 {14,-9,899,0}
};
static const re3c_i16 damage_gd_rest[17][4]={
 {-1,0,-788,0},
 {0,-954,18,0},
 {1,-899,-50,0},
 {2,-1338,46,0},
 {3,-1385,104,0},
 {4,-1399,112,0},
 {5,-1342,138,0},
 {0,960,-32,0},
 {7,705,52,0},
 {8,499,-413,-431},
 {9,759,-481,-481},
 {8,499,449,-431},
 {11,759,481,-481},
 {8,499,-413,431},
 {13,759,-481,481},
 {8,499,449,431},
 {15,759,481,481}
};
static int damage_rest_matches(const re3c_model *model,const re3c_i16 (*ref)[4],re3c_u32 count){
 re3c_u32 j;
 const re3c_anim_bank *norm,*damage;
 if(!model||!model->is_open)return 0;
 norm=&model->banks[RE3C_BANK_A];damage=&model->banks[RE3C_BANK_DAMAGE];
 if(!norm->present||!damage->present||norm->joint_count!=count||damage->joint_count!=15u)return 0;
 if(norm->hierarchy_source!=RE3C_BANK_A ||damage->hierarchy_source!=255u)return 0;
 if(!model->bind_a.present||!model->bind_a.inverse_bind_valid||model->bind_a.joint_count!=count)return 0;
 for(j=0u;j<count;j++){
  const re3c_joint *a=&norm->joints[j];
  if(a->parent!=ref[j][0]||
   a->local_pos.x!=re3c_fx_from_int(ref[j][1])||
   a->local_pos.y!=re3c_fx_from_int(ref[j][2])||
   a->local_pos.z!=re3c_fx_from_int(ref[j][3]))return 0;
 }
 return 1;
}
int re3c_damage_family_check(const re3c_model*model,re3c_u32 family){
 if(!model)return RE3C_ERR_BAD_ARGUMENT;
 if(!model->is_open)return RE3C_ERR_NOT_OPEN;
 if(family==RE3C_DAMAGE_FAMILY_NEMESIS)
  return damage_rest_matches(model,damage_nm_rest,16u)?RE3C_OK:RE3C_ERR_NOT_PRESENT;
 if(family==RE3C_DAMAGE_FAMILY_GRAVEDIGGER)
  return damage_rest_matches(model,damage_gd_rest,17u)?RE3C_OK:RE3C_ERR_NOT_PRESENT;
 return RE3C_ERR_BAD_ARGUMENT;
}
int re3c_retarget_nemesis_damage_experimental(const re3c_model*model,const re3c_pose*src,re3c_pose*dst){
 static const re3c_u16 map[16]={0u,RE3C_JOINT_NEUTRAL,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u};
 int r;if(!model||!src||!dst)return RE3C_ERR_BAD_ARGUMENT;
 r=re3c_damage_family_check(model,RE3C_DAMAGE_FAMILY_NEMESIS);if(r!=RE3C_OK)return r;
 if(src->joint_count!=15u)return RE3C_ERR_NOT_PRESENT;
 return re3c_retarget_pose_explicit(src,&model->banks[RE3C_BANK_A],map,dst);
}
int re3c_retarget_gravedigger_damage_experimental(const re3c_model*model,const re3c_pose*src,re3c_pose*dst){
 /* Trial projection only: channels 0..14 drive the first 15 joints;
  * two distal appendage joints remain neutral. No semantic claim. */
 static const re3c_u16 map[17]={0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,RE3C_JOINT_NEUTRAL,RE3C_JOINT_NEUTRAL};
 int r;if(!model||!src||!dst)return RE3C_ERR_BAD_ARGUMENT;
 r=re3c_damage_family_check(model,RE3C_DAMAGE_FAMILY_GRAVEDIGGER);if(r!=RE3C_OK)return r;
 if(src->joint_count!=15u)return RE3C_ERR_NOT_PRESENT;
 return re3c_retarget_pose_explicit(src,&model->banks[RE3C_BANK_A],map,dst);
}
static int damage_section_match(const re3c_model*a,const re3c_model*b,re3c_u32 sec,int*equal){
 re3c_u8 aa[1024],bb[1024];re3c_u32 off=0u,sz;
 if(!a->sections[sec].present||!b->sections[sec].present)return RE3C_ERR_NOT_PRESENT;
 if(a->sections[sec].size!=b->sections[sec].size){*equal=0;return RE3C_OK;}
 sz=a->sections[sec].size;
 while(off<sz){re3c_u32 n=sz-off>1024u?1024u:sz-off;int r;
  r=re3c_read_section(a,sec,off,aa,(size_t)n);if(r!=RE3C_OK)return r;
  r=re3c_read_section(b,sec,off,bb,(size_t)n);if(r!=RE3C_OK)return r;
  if(memcmp(aa,bb,(size_t)n)!=0){*equal=0;return RE3C_OK;}
  off+=n;
 }
 return RE3C_OK;
}
int re3c_damage_sections_equal(const re3c_model*a,const re3c_model*b,int*equal){
 int r;if(!a||!b||!equal)return RE3C_ERR_BAD_ARGUMENT;
 if(!a->is_open||!b->is_open)return RE3C_ERR_NOT_OPEN;
 *equal=1;
 r=damage_section_match(a,b,RE3C_SEC_EDD_DAMAGE,equal);
 if(r!=RE3C_OK||!*equal)return r;
 return damage_section_match(a,b,RE3C_SEC_EMR_DAMAGE,equal);
}
int re3c_normal_rigs_equal(const re3c_model*a,const re3c_model*b,int*equal){
 const re3c_anim_bank*x,*y;re3c_u32 j;
 if(!a||!b||!equal)return RE3C_ERR_BAD_ARGUMENT;
 if(!a->is_open||!b->is_open)return RE3C_ERR_NOT_OPEN;
 x=&a->banks[RE3C_BANK_A];y=&b->banks[RE3C_BANK_A];
 if(!x->present||!y->present)return RE3C_ERR_NOT_PRESENT;
 *equal=0;
 if(x->joint_count!=y->joint_count)return RE3C_OK;
 for(j=0u;j<x->joint_count;j++){
  const re3c_joint *p=&x->joints[j],*q=&y->joints[j];
  if(p->parent!=q->parent||p->local_pos.x!=q->local_pos.x||p->local_pos.y!=q->local_pos.y||p->local_pos.z!=q->local_pos.z)return RE3C_OK;
 }
 *equal=1;return RE3C_OK;
}

/* v0.27: distinguish valid frames from a valid target-joint binding.
 * A 15-channel frame bank with an 8-byte-only EMR header is not a
 * self-describing skeleton and must not be rebound implicitly. */
int re3c_damage_rig_status(const re3c_model *model, re3c_u32 *status){
 const re3c_anim_bank *normal,*damage;
 if(!model||!status)return RE3C_ERR_BAD_ARGUMENT;
 if(!model->is_open)return RE3C_ERR_NOT_OPEN;
 normal=&model->banks[RE3C_BANK_A];
 damage=&model->banks[RE3C_BANK_DAMAGE];
 if(!damage->present){*status=RE3C_DAMAGE_RIG_ABSENT;return RE3C_OK;}
 *status=RE3C_DAMAGE_RIG_UNRESOLVED;
 if(normal->present&&damage->hierarchy_inherited &&
    damage->hierarchy_source==RE3C_BANK_A &&
    damage->joint_count==normal->joint_count &&
    damage->joint_count>0u){
  *status=RE3C_DAMAGE_RIG_INHERITED;
 }
 return RE3C_OK;
}

/* v0.28: RE3 x86 sampler loops iterate the EMR's channel count and write
 * one 0xBC-byte node per channel. Preserve unmatched nodes from caller's
 * previous/reference pose rather than inventing a retarget table. */
int re3c_damage_overlay_prefix(const re3c_model *model,
 const re3c_pose *base, const re3c_pose *damage, re3c_pose *dst){
 const re3c_anim_bank *normal,*bank;re3c_u32 j;
 if(!model||!base||!damage||!dst)return RE3C_ERR_BAD_ARGUMENT;
 if(!model->is_open||damage==dst)return RE3C_ERR_BAD_ARGUMENT;
 normal=&model->banks[RE3C_BANK_A];bank=&model->banks[RE3C_BANK_DAMAGE];
 if(!normal->present||!bank->present)return RE3C_ERR_NOT_PRESENT;
 if(normal->joint_count==0u||bank->joint_count==0u||
    bank->joint_count>normal->joint_count||
    base->joint_count!=normal->joint_count||
    damage->joint_count!=bank->joint_count)return RE3C_ERR_NOT_PRESENT;
 *dst=*base;
 for(j=0u;j<bank->joint_count;j++){
  dst->rot_x[j]=damage->rot_x[j];
  dst->rot_y[j]=damage->rot_y[j];
  dst->rot_z[j]=damage->rot_z[j];
 }
 dst->speed=damage->speed;
 dst->origin_y=damage->origin_y;
 return RE3C_OK;
}
