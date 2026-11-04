#include "c4d89_internal.h"
#include <string.h>
#include <ctype.h>

static int c4d89_match_suffix(const char *s,const char *ext)
{
    unsigned int n=(unsigned int)strlen(s), k=(unsigned int)strlen(ext),i;
    if (n<k) return 0;
    for(i=0u;i<k;i++)
        if (tolower((unsigned char)s[n-k+i])!=tolower((unsigned char)ext[i])) return 0;
    return 1;
}
static int c4d89_is_tex(const char *s)
{
    return c4d89_match_suffix(s,".jpg") || c4d89_match_suffix(s,".jpeg") ||
      c4d89_match_suffix(s,".png") || c4d89_match_suffix(s,".tga") ||
      c4d89_match_suffix(s,".bmp") || c4d89_match_suffix(s,".tif") ||
      c4d89_match_suffix(s,".tiff") || c4d89_match_suffix(s,".dds");
}
/* Finds typed UCS-2BE/UTF-16BE ASCII subset, where type markers 0x82
   and 0x83 0x83 were observed in provided C4D6 samples. It does not
   attempt to parse the surrounding object graph and makes NO claims
   that a recovered string is necessarily an object or material name. */
int c4d89_scan_names(FILE *f,long filesize,C4D89_Report *rep)
{
    int c; long pos;
    unsigned char sz[4]; unsigned long bytes;
    char name[C4D89_MAX_NAME]; unsigned int j, chars;
    if (fseek(f,8L,SEEK_SET)!=0) return C4D89_ERR_IO;
    while((pos=ftell(f))>=0L && pos < filesize-8L && (c=fgetc(f))!=EOF) {
        int x,valid=1;
        if(c!=0x82 && c!=0x83) continue;
        if(c==0x83) { x=fgetc(f); if(x!=0x83) {fseek(f,pos+1L,SEEK_SET);continue;} }
        if (fread(sz,1u,4u,f)!=4u) return C4D89_OK;
        bytes=c4d89_be32(sz);
        if(bytes<4ul || bytes>2ul*(C4D89_MAX_NAME-1u) || (bytes&1ul)) {
            fseek(f,pos+1L,SEEK_SET); continue;
        }
        if ((long)bytes>filesize-ftell(f)) { fseek(f,pos+1L,SEEK_SET);continue; }
        chars=(unsigned int)(bytes/2ul);
        for(j=0u;j<chars;j++) {
            int hi=fgetc(f),lo=fgetc(f);
            if(hi!=0 || lo<32 || lo>126) {valid=0;break;}
            name[j]=(char)lo;
        }
        if(valid && chars>=2u) {
            unsigned int i,dup=0u;
            name[chars]='\0';
            for(i=0u;i<rep->name_count;i++) {
                if(strcmp(rep->names[i].text,name)==0) {dup=1u;break;}
            }
            if(!dup) {
                if (rep->name_count<C4D89_MAX_NAMES) {
                    C4D89_Name *entry=&rep->names[rep->name_count++];
                    memcpy(entry->text,name,chars+1u);
                    entry->offset=(unsigned long)pos;
                    entry->is_texture_reference=(unsigned int)c4d89_is_tex(name);
                } else rep->names_over_limit++;
            }
        }
        /* Skip the marker minimally for invalid candidates. */
        if(!valid) fseek(f,pos+1L,SEEK_SET);
    }
    return ferror(f) ? C4D89_ERR_IO : C4D89_OK;
}
