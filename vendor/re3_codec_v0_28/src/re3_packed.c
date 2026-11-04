/* RE3 PLD/PLW containers: conservative read-only support, C89. */
#include "re3_packed.h"
#include <string.h>
#include <limits.h>
static re3c_u32 getu32(const re3c_u8 *b){
 return (re3c_u32)b[0]|((re3c_u32)b[1]<<8)|((re3c_u32)b[2]<<16)|((re3c_u32)b[3]<<24);
}
static int rd(FILE *f,re3c_u32 offset,void *p,size_t n){
 if(offset>(re3c_u32)LONG_MAX)return RE3C_ERR_RANGE;
 if(fseek(f,(long)offset,SEEK_SET)!=0)return RE3C_ERR_IO;
 if(n && fread(p,1u,n,f)!=n)return RE3C_ERR_IO;
 return RE3C_OK;
}
static int filelength(FILE*f,re3c_u32*size){long n;
 if(fseek(f,0L,SEEK_END)!=0)return RE3C_ERR_IO;
 n=ftell(f);if(n<8L||(unsigned long)n>0xFFFFFFFFUL)return RE3C_ERR_OVERFLOW;
 *size=(re3c_u32)n;return RE3C_OK;
}
static int probe(FILE*f,re3p_info*out){
 re3c_u8 header[8],item[4];re3c_u32 i,v,last,n;int rc;
 memset(out,0,sizeof(*out));rc=filelength(f,&out->file_size);if(rc!=RE3C_OK)return rc;
 rc=rd(f,0u,header,8u);if(rc!=RE3C_OK)return rc;
 out->directory_offset=getu32(header);n=getu32(header+4u);
 if(n!=RE3P_KIND_PLD&&n!=RE3P_KIND_PLW)return RE3C_ERR_BAD_FORMAT;
 if(out->directory_offset<8u||out->directory_offset>out->file_size||
   out->file_size-out->directory_offset!=n*4u)return RE3C_ERR_BAD_FORMAT;
 out->kind=n;out->segment_count=n;last=0u;
 for(i=0u;i<n;i++){
  rc=rd(f,out->directory_offset+i*4u,item,4u);if(rc!=RE3C_OK)return rc;
  v=getu32(item);
  if(v<8u||v>=out->directory_offset||(i==0u&&v!=8u)||
      (i>0u&&v<=last))return RE3C_ERR_BAD_FORMAT;
  out->segments[i].offset=v;
  if(i>0u)out->segments[i-1u].size=v-last;
  last=v;
 }
 out->segments[n-1u].size=out->directory_offset-last;
 return RE3C_OK;
}
int re3p_probe_file(const char*path,re3p_info*out){FILE*f;int rc;
 if(!path||!out)return RE3C_ERR_BAD_ARGUMENT;
 f=fopen(path,"rb");if(!f)return RE3C_ERR_IO;
 rc=probe(f,out);fclose(f);return rc;
}
int re3p_open_model_file(re3c_model*m,const char*path,re3p_info*info){
 FILE*f;re3p_info local;int r;re3c_u32 md2,edd,emr,dat;
 if(!m||!path)return RE3C_ERR_BAD_ARGUMENT;
 re3c_model_close(m);f=fopen(path,"rb");if(!f)return RE3C_ERR_IO;
 r=probe(f,&local);if(r!=RE3C_OK){fclose(f);return r;}
 if(info)*info=local;
 md2=(local.kind==RE3P_KIND_PLD)?2u:7u;
 edd=0u;emr=1u;dat=(local.kind==RE3P_KIND_PLD)?3u:local.segment_count;
 m->source.fp=f;m->source.size=local.file_size;m->source.owns_file=1;
 m->section_count=RE3C_MAX_SECTIONS;
 m->index_offset=local.directory_offset;
 m->sections[RE3C_SEC_MD2_BODY].offset=local.segments[md2].offset;
 m->sections[RE3C_SEC_MD2_BODY].size=local.segments[md2].size;
 m->sections[RE3C_SEC_MD2_BODY].present=1u;
 m->sections[RE3C_SEC_EDD_A].offset=local.segments[edd].offset;
 m->sections[RE3C_SEC_EDD_A].size=local.segments[edd].size;
 m->sections[RE3C_SEC_EDD_A].present=1u;
 m->sections[RE3C_SEC_EMR_A].offset=local.segments[emr].offset;
 m->sections[RE3C_SEC_EMR_A].size=local.segments[emr].size;
 m->sections[RE3C_SEC_EMR_A].present=1u;
 /* The DAT chunk is not mapped as enemy binary00 until validated.
  * Wrong ownership maps would be worse than a conservative rigid fallback. */
 (void)dat;
 m->is_open=1u;
 r=re3c_parse_body_mesh(m);
 if(r!=RE3C_OK){re3c_model_close(m);return r;}
 /* PLD/PLW animation banks may have player-specific conventions. Their
  * reconstruction is best effort; no fake animation is generated. */
 if(local.kind==RE3P_KIND_PLD){
  r=re3c_parse_all_banks(m);
  if(r!=RE3C_OK && r!=RE3C_ERR_NOT_PRESENT){
   /* Keep a usable geometry-only model if the EMR layout differs. */
   memset(&m->banks[RE3C_BANK_A],0,sizeof(m->banks[RE3C_BANK_A]));
   m->banks[RE3C_BANK_A].bank_id=RE3C_BANK_A;
  }
 }
 /* PLW sections 0/1 contain animation-like data but refer to external player
  * skeleton state; treating them as independent EMD animations produces an
  * invalid hierarchy. Do not fabricate a compatible skeleton. */
 if(local.kind==RE3P_KIND_PLD){
  /* PLD DAT stream: attempt the existing ownership-map decoder.
   * Unrecognized variants are left as rigid skeleton data. */
  m->sections[RE3C_SEC_BINARY00].offset=local.segments[dat].offset;
  m->sections[RE3C_SEC_BINARY00].size=local.segments[dat].size;
  m->sections[RE3C_SEC_BINARY00].present=1u;
  r=re3c_parse_bind_banks(m);
  if(r!=RE3C_OK){
   memset(&m->sections[RE3C_SEC_BINARY00],0,sizeof(m->sections[RE3C_SEC_BINARY00]));
   /* parse_bind may partially initialize; clear it before classifying */
   m->bind_a.present=0u;
  }
 }
 r=re3c_classify_body_parts(m);
 if(r!=RE3C_OK){re3c_model_close(m);return r;}
 if(local.kind==RE3P_KIND_PLW){
  re3c_u32 pi;
  /* PLW body mesh is weapon geometry, not an 'optional enemy morph'.
   * Mark visible without fabricating an independent bone hierarchy. */
  for(pi=0u;pi<m->body.part_count;pi++){
   re3c_mesh_part *part=&m->body.parts[pi];
   part->part_flags|=RE3C_PART_FLAG_RENDER_DEFAULT;
   part->part_class=RE3C_PART_RIGID_JOINT;
   part->driver_joint=-1;
  }
 }
 return RE3C_OK;
}
int re3p_load_tim(const char*path,const re3p_info*info,re3c_u8*scratch,
                   size_t capacity,re3c_tim_vram*tim){
 FILE*f;re3c_u32 i,n;int rc;
 if(!path||!info||!scratch||!tim)return RE3C_ERR_BAD_ARGUMENT;
 if(info->kind!=RE3P_KIND_PLD&&info->kind!=RE3P_KIND_PLW)return RE3C_ERR_BAD_FORMAT;
 i=info->kind-1u;n=info->segments[i].size;
 if((size_t)n>capacity)return RE3C_ERR_OVERFLOW;
 f=fopen(path,"rb");if(!f)return RE3C_ERR_IO;
 rc=rd(f,info->segments[i].offset,scratch,(size_t)n);fclose(f);
 if(rc!=RE3C_OK)return rc;
 return re3c_tim_load_memory(tim,scratch,(size_t)n);
}

/* v0.25: VRAM texture composition for PLD + PLW. Do not rebase the UVs:
 * PLW polygons can reference the PLAYER atlas rather than the PLW TIM.
 * Populate only declared TIM rectangles, leaving unrelated VRAM untouched. */
static int re3p_validate_rect(const re3c_tim_rect *r){
 if(!r||!r->width_words||!r->height)return RE3C_ERR_BAD_FORMAT;
 if(r->x>=RE3C_TIM_VRAM_W||r->y>=RE3C_TIM_VRAM_H)return RE3C_ERR_RANGE;
 if((re3c_u32)r->width_words>RE3C_TIM_VRAM_W-(re3c_u32)r->x)return RE3C_ERR_RANGE;
 if((re3c_u32)r->height>RE3C_TIM_VRAM_H-(re3c_u32)r->y)return RE3C_ERR_RANGE;
 return RE3C_OK;
}
static int re3p_copy_rect(re3c_tim_vram *out,const re3c_tim_vram *src,
                           const re3c_tim_rect *r){
 re3c_u32 y;int rc=re3p_validate_rect(r);if(rc!=RE3C_OK)return rc;
 for(y=0u;y<(re3c_u32)r->height;y++){
  size_t pos=(size_t)(r->y+y)*(size_t)RE3C_TIM_VRAM_W+(size_t)r->x;
  memcpy(out->words+pos,src->words+pos,(size_t)r->width_words*sizeof(re3c_u16));
 }
 return RE3C_OK;
}
int re3p_compose_vram(re3c_tim_vram *dst,
                      const re3c_tim_vram *player,
                      const re3c_tim_vram *weapon){
 int rc;if(!dst||!player||!weapon)return RE3C_ERR_BAD_ARGUMENT;
 if(dst==player||dst==weapon||player==weapon)return RE3C_ERR_BAD_ARGUMENT;
 if(!player->loaded||!weapon->loaded)return RE3C_ERR_NOT_OPEN;
 rc=re3p_validate_rect(&player->image_rect);if(rc!=RE3C_OK)return rc;
 rc=re3p_validate_rect(&weapon->image_rect);if(rc!=RE3C_OK)return rc;
 if(player->has_clut){rc=re3p_validate_rect(&player->clut_rect);if(rc!=RE3C_OK)return rc;}
 if(weapon->has_clut){rc=re3p_validate_rect(&weapon->clut_rect);if(rc!=RE3C_OK)return rc;}
 *dst=*player;
 rc=re3p_copy_rect(dst,weapon,&weapon->image_rect);if(rc!=RE3C_OK)return rc;
 if(weapon->has_clut){rc=re3p_copy_rect(dst,weapon,&weapon->clut_rect);if(rc!=RE3C_OK)return rc;}
 return RE3C_OK;
}
