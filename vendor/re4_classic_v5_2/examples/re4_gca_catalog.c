/* Standalone file-access demo for the caller-buffer-only C89 GCAX library.
   Never constructs output paths from names stored in untrusted archives. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define FOOTER_CAP (4UL * 1024UL * 1024UL)
#define ARENA_CAP (4UL * 1024UL * 1024UL)
#define COPY_CAP (64UL * 1024UL)
static re4_u8 g_footer[FOOTER_CAP];
static re4_u8 g_arena_mem[ARENA_CAP];
static re4_u8 g_copy[COPY_CAP];

static int name_eq(re4_strref ref, const char *name)
{
    re4_size n = (re4_size)strlen(name);
    return ref.len == n && memcmp(ref.ptr, name, (size_t)n) == 0;
}

static void print_name(re4_strref s)
{
    re4_size i;
    for(i=0UL;i<s.len;++i) {
        unsigned char c=(unsigned char)s.ptr[i];
        if (c >= 32U && c <= 126U) putchar((int)c);
        else putchar('?');
    }
}

static int copy_entry(FILE *in, const re4_archive_entry *e, const char *path)
{
    FILE *out;
    re4_size left, n;
#if LONG_MAX < 4294967295UL
    if (e->offset > (re4_u32)LONG_MAX) return 0;
#endif
    if (fseek(in,(long)e->offset,SEEK_SET)!=0) return 0;
    out=fopen(path,"wb");
    if (!out) return 0;
    left=(re4_size)e->packed_size;
    while(left!=0UL) {
        n=left>COPY_CAP ? COPY_CAP:left;
        if (fread(g_copy,1U,(size_t)n,in)!=(size_t)n ||
            fwrite(g_copy,1U,(size_t)n,out)!=(size_t)n) {
            fclose(out);return 0;
        }
        left-=n;
    }
    if (fclose(out)!=0) return 0;
    return 1;
}

int main(int argc, char **argv)
{
    FILE *in;
    long file_size;
    re4_u8 h[16];
    re4_u32 offset, footer_size, i, matched;
    re4_archive archive;
    re4_span meta;
    re4_arena arena;
    re4_err er;
    int list,extract;
    if (argc < 2 || (argc > 2 && strcmp(argv[2],"--list")!=0 && strcmp(argv[2],"--extract")!=0)) {
        fprintf(stderr,"usage: re4_gca_catalog FILE [--list | --extract INTERNAL_NAME OUTPUT_FILE]\n");return 1;
    }
    list=(argc==3 && strcmp(argv[2],"--list")==0);
    extract=(argc==5 && strcmp(argv[2],"--extract")==0);
    if (argc!=2 && !list && !extract) return 1;
    in=fopen(argv[1],"rb");
    if(!in){perror("open");return 2;}
    if(fseek(in,0L,SEEK_END)!=0 || (file_size=ftell(in))<0L ||
       (unsigned long)file_size>0xFFFFFFFFUL || fseek(in,0L,SEEK_SET)!=0 ||
       fread(h,1U,16U,in)!=16U || memcmp(h,"GCAX",4)!=0) {
        fprintf(stderr,"not a valid GCAX file/header\n");fclose(in);return 3;
    }
    if(re4_read_u32_le(h+12)!=0U) {fclose(in);return 4;}
    offset=re4_read_u32_le(h+8);
    if(offset<16U || offset>(re4_u32)file_size){fclose(in);return 4;}
    footer_size=(re4_u32)file_size-offset;
    if ((re4_size)footer_size > FOOTER_CAP ||
        fseek(in,(long)offset,SEEK_SET)!=0 ||
        fread(g_footer,1U,(size_t)footer_size,in)!=(size_t)footer_size) {
        fprintf(stderr,"GCA3 footer too large or unreadable\n");fclose(in);return 5;
    }
    meta.data=g_footer;meta.size=(re4_size)footer_size;
    re4_arena_init(&arena,g_arena_mem,ARENA_CAP);
    er=re4_gca_read_footer(meta,(re4_u32)file_size,offset,&arena,&archive);
    if(er!=RE4_OK){fprintf(stderr,"GCA3 parse error: %s\n",re4_err_name(er));fclose(in);return 6;}
    printf("GCAX OK: %s bytes=%lu files=%u footer=0x%08lX footer_bytes=%u\n",
         argv[1],(unsigned long)file_size,(unsigned)archive.entry_count,
         (unsigned long)offset,(unsigned)footer_size);
    if (list) {
        for(i=0U;i<archive.entry_count;++i){
            printf("%4u  %10u  %10u  flags=0x%X  ",(unsigned)i,
                (unsigned)archive.entries[i].offset,
                (unsigned)archive.entries[i].packed_size,
                (unsigned)archive.entries[i].flags);
            print_name(archive.entries[i].name);putchar('\n');
        }
    }
    if (extract) {
        matched=0U;
        for(i=0U;i<archive.entry_count;++i) {
            if(name_eq(archive.entries[i].name,argv[3])) {
                ++matched;
                if(!copy_entry(in,&archive.entries[i],argv[4])) {
                    fprintf(stderr,"extraction failed\n");fclose(in);return 7;
                }
                printf("extracted %u bytes to %s\n",(unsigned)archive.entries[i].packed_size,argv[4]);
                break;
            }
        }
        if(!matched){fprintf(stderr,"internal file not found\n");fclose(in);return 8;}
    }
    fclose(in);return 0;
}
