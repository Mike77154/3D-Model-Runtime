#include <stdio.h>
#include <string.h>
#include "xp.h"

typedef struct probe_state {
    unsigned int dup_begin;
    unsigned int dup_items;
    unsigned int decl_begin;
    unsigned int decl_items;
    unsigned int decl_words;
    unsigned int fvf_begin;
    unsigned int fvf_words;
    unsigned int unknown_begin;
    unsigned int unknown_tokens;
    unsigned int diagnostic_count;
} probe_state;

typedef struct chunk_state {
    const unsigned char* data;
    unsigned long size;
    unsigned long pos;
    unsigned int chunk;
} chunk_state;

static void dup_begin(void* u, unsigned int n, unsigned int o) {
    probe_state* p=(probe_state*)u;
    if (n==4u && o==3u) p->dup_begin++;
}
static void dup_item(void* u, unsigned int i, unsigned int v) {
    probe_state* p=(probe_state*)u;
    if (i<4u && v<3u) p->dup_items++;
}
static void decl_begin(void* u, unsigned int n) {
    probe_state* p=(probe_state*)u;
    if (n==1u) p->decl_begin++;
}
static void decl_item(void* u, unsigned int i, unsigned int t, unsigned int m,
                      unsigned int use, unsigned int ui) {
    probe_state* p=(probe_state*)u;
    if (i==0u && t==2u && m==0u && use==3u && ui==0u) p->decl_items++;
}
static void decl_word(void* u, unsigned int i, unsigned int v) {
    probe_state* p=(probe_state*)u;
    if ((i==0u && v==123u) || (i==1u && v==456u) ||
        (i==2u && v==0xffffffffu)) p->decl_words++;
}
static void fvf_begin(void* u, unsigned int fvf, unsigned int n) {
    probe_state* p=(probe_state*)u;
    if (fvf==258u && n==2u) p->fvf_begin++;
}
static void fvf_word(void* u, unsigned int i, unsigned int v) {
    probe_state* p=(probe_state*)u;
    if ((i==0u && v==11u) || (i==1u && v==22u)) p->fvf_words++;
}
static void unknown_begin(void* u, const char* t, const char* n) {
    probe_state* p=(probe_state*)u;
    if (strcmp(t,"CustomRuntimeData")==0 && strcmp(n,"Example")==0) p->unknown_begin++;
}
static void unknown_tok(void* u, const xp_unknown_token* tok) {
    probe_state* p=(probe_state*)u;
    if (tok && tok->kind!=XP_UNKNOWN_EOF) p->unknown_tokens++;
}
static void diagnostic(void* u, const xp_diagnostic* d) {
    probe_state* p=(probe_state*)u;
    if (d && d->result!=XP_OK) p->diagnostic_count++;
}

static xp_result chunk_read(void* u, unsigned char* dst, unsigned int cap, unsigned int* out_n) {
    chunk_state* s=(chunk_state*)u;
    unsigned long left;
    unsigned int n;
    if (!s || !dst || !out_n) return XP_EFORMAT;
    if (s->pos>s->size) return XP_EFORMAT;
    left=s->size-s->pos;
    n=cap;
    if (n>s->chunk) n=s->chunk;
    if ((unsigned long)n>left) n=(unsigned int)left;
    if (n) memcpy(dst,s->data+s->pos,n);
    s->pos+=(unsigned long)n;
    *out_n=n;
    return XP_OK;
}

static int load_file(const char* path, unsigned char* buf, unsigned long cap, unsigned long* out_n) {
    FILE* f;
    size_t n;
    f=fopen(path,"rb");
    if (!f) return 0;
    n=fread(buf,1,(size_t)cap,f);
    if (ferror(f)) { fclose(f); return 0; }
    if (!feof(f)) { fclose(f); return 0; }
    fclose(f);
    *out_n=(unsigned long)n;
    return 1;
}

static int parse_buffer(const unsigned char* data, unsigned long size, int chunked) {
    xp_context ctx;
    xp_header hdr;
    xp_callbacks cb;
    xp_io io;
    xp_memory_io mem;
    chunk_state cs;
    probe_state p;
    xp_result r;
    memset(&p,0,sizeof(p));
    memset(&cb,0,sizeof(cb));
    cb.on_vertex_duplication_begin=dup_begin;
    cb.on_vertex_duplication_index=dup_item;
    cb.on_decl_data_begin=decl_begin;
    cb.on_decl_element=decl_item;
    cb.on_decl_data_dword=decl_word;
    cb.on_fvf_data_begin=fvf_begin;
    cb.on_fvf_data_dword=fvf_word;
    cb.on_unknown_object_begin=unknown_begin;
    cb.on_unknown_token=unknown_tok;
    cb.on_diagnostic=diagnostic;
    if (chunked) {
        cs.data=data;cs.size=size;cs.pos=0UL;cs.chunk=3u;
        io.user=&cs;io.read=chunk_read;
    } else {
        xp_io_init_memory(&io,&mem,data,size);
    }
    r=xp_open_io(&ctx,&io,&hdr);
    if (r!=XP_OK) return 0;
    r=xp_parse(&ctx,&cb,&p);
    if (r!=XP_OK) return 0;
    if (p.dup_begin!=1u || p.dup_items!=4u || p.decl_begin!=1u ||
        p.decl_items!=1u || p.decl_words!=3u || p.fvf_begin!=1u ||
        p.fvf_words!=2u || p.unknown_begin!=1u || p.unknown_tokens==0u ||
        p.diagnostic_count!=0u) return 0;
    return 1;
}

static int budget_test(const unsigned char* data, unsigned long size) {
    xp_context ctx;
    xp_header hdr;
    xp_callbacks cb;
    xp_io io;
    xp_memory_io mem;
    xp_limits lim;
    probe_state p;
    xp_result r;
    memset(&cb,0,sizeof(cb));
    memset(&p,0,sizeof(p));
    cb.on_diagnostic=diagnostic;
    xp_io_init_memory(&io,&mem,data,size);
    r=xp_open_io(&ctx,&io,&hdr);
    if (r!=XP_OK) return 0;
    xp_limits_unlimited(&lim);
    lim.max_objects=1UL;
    r=xp_set_limits(&ctx,&lim);
    if (r!=XP_OK) return 0;
    r=xp_parse(&ctx,&cb,&p);
    if (r!=XP_ELIMIT || p.diagnostic_count!=1u) return 0;
    if (!xp_get_last_diagnostic(&ctx)) return 0;
    if (xp_get_last_diagnostic(&ctx)->result!=XP_ELIMIT) return 0;
    return 1;
}

int main(void) {
    static unsigned char textbuf[65536];
    static unsigned char binbuf[65536];
    static unsigned char zipbuf[1048576];
    unsigned long textn, binn, zipn;
    xp_context ctx;
    xp_header hdr;
    xp_io io;
    xp_memory_io mem;
    xp_result r;

    if (!load_file("tests/fixtures/runtime_v04_extensions_text.x",textbuf,sizeof(textbuf),&textn)) return 10;
    if (!load_file("tests/fixtures/runtime_v04_extensions_binary.x",binbuf,sizeof(binbuf),&binn)) return 11;
    if (!load_file("tests/fixtures/runtime_v03_bzip_standard.x",zipbuf,sizeof(zipbuf),&zipn)) return 12;
    if (!parse_buffer(textbuf,textn,0)) return 20;
    if (!parse_buffer(textbuf,textn,1)) return 21;
    if (!parse_buffer(binbuf,binn,0)) return 22;
    if (!parse_buffer(binbuf,binn,1)) return 23;
    if (!budget_test(textbuf,textn)) return 24;

    xp_io_init_memory(&io,&mem,zipbuf,zipn);
    r=xp_open_io(&ctx,&io,&hdr);
    if (r!=XP_OK) return 30;
    r=xp_parse(&ctx,(const xp_callbacks*)0,(void*)0);
    if (r!=XP_OK) return 31;

    puts("v0.4 tests: OK");
    return 0;
}
