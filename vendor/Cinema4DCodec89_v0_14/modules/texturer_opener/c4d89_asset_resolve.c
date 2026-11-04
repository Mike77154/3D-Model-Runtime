#include "c4d89.h"
#include <string.h>
/* Standalone material bitmap *file* resolver. This is deliberately NOT a
   guess at the binary material BaseLink. No malloc, float, 64-bit, filesystem
   APIs, scene-specific file names or platform-specific dependencies. */
static int ascii_lower(int c)
{
    if(c>='A'&&c<='Z')return c+('a'-'A');
    return c;
}
static int equal_ascii(const char *a,const char *b)
{
    while(*a&&*b){
        if(ascii_lower((unsigned char)*a)!=ascii_lower((unsigned char)*b))return 0;
        a++;b++;
    }
    return *a==*b;
}
static int clean_copy(const char *in,char *out,unsigned int max)
{
    unsigned int i=0u,j=0u;
    if(!in||!out||max<2u||!*in)return 0;
    if(in[0]=='/'||in[0]=='\\')return 0;
    /* Reject traversal and drive prefixes rather than guessing a relative
       path. Backslash is only a separator. */
    while(in[i]){
        unsigned int c=(unsigned char)in[i++];
        if(i>=max || c<32u || c==127u || c==':')return 0;
        if(c=='\\')c='/';
        if(c=='/' && (j==0u || out[j-1u]=='/'))continue;
        out[j++]=(char)c;
    }
    if(j==0u || out[j-1u]=='/')return 0;
    out[j]='\0';
    {
        const char *p=out;
        while(*p){
            const char *q=strchr(p,'/');
            unsigned int n=q?(unsigned int)(q-p):(unsigned int)strlen(p);
            if((n==1u&&p[0]=='.') ||
               (n==2u&&p[0]=='.'&&p[1]=='.'))return 0;
            if(!q)break;
            p=q+1;
        }
    }
    return 1;
}
static const char *base_name(const char *s)
{
    const char *q=strrchr(s,'/');return q?q+1:s;
}
static int same_folder(const char *scene,const char *image,int tex)
{
    const char *last=strrchr(scene,'/');
    unsigned int dirlen=last?(unsigned int)(last-scene)+1u:0u;
    const char *remaining;
    {
        unsigned int k;
        for(k=0u;k<dirlen;k++)
            if(!image[k] || ascii_lower((unsigned char)scene[k])!=
                    ascii_lower((unsigned char)image[k]))return 0;
    }
    remaining=image+dirlen;
    if(tex){
        if(!(ascii_lower((unsigned char)remaining[0])=='t'&&
             ascii_lower((unsigned char)remaining[1])=='e'&&
             ascii_lower((unsigned char)remaining[2])=='x'&&
             remaining[3]=='/'))return 0;
        remaining+=4;
    }
    /* Only a direct child counts, never another character's subdirectory. */
    if(strchr(remaining,'/')!=0)return 0;
    return 1;
}
int c4d89_match_bitmap_asset(const char *scene_relative_path,
         const char *bitmap_reference,const C4D89_ResourceIndex *index,
         C4D89_ResourceMatch *result)
{
    char scene[C4D89_RESOURCE_PATH_MAX];
    char reference[C4D89_RESOURCE_PATH_MAX];
    char candidate[C4D89_RESOURCE_PATH_MAX];
    const char *refbase;unsigned int i,level,matched=0u;
    unsigned int counts[3]={0u,0u,0u};
    unsigned int idx[3]={0u,0u,0u};
    char matched_path[3][C4D89_RESOURCE_PATH_MAX];
    if(!index||!result||!index->get)return C4D89_ERR_CORRUPT;
    memset(result,0,sizeof(*result));
    result->matched_index=(unsigned int)-1;
    if(!clean_copy(scene_relative_path,scene,C4D89_RESOURCE_PATH_MAX)){
        result->status=C4D89_ASSET_BAD_PATH;return C4D89_OK;
    }
    if(!clean_copy(bitmap_reference,reference,C4D89_RESOURCE_PATH_MAX)){
        /* Legacy authoring paths such as F:\...\shark.arc\textures\file.tga
           are NOT trusted filesystem paths. Only their final filename is used
           for matching against the engine-supplied asset index. */
        if(bitmap_reference && bitmap_reference[0] &&
           bitmap_reference[1]==':' &&
           (bitmap_reference[2]=='\\'||bitmap_reference[2]=='/')){
            const char *q=bitmap_reference,*p=bitmap_reference;
            while(*q){if(*q=='/'||*q=='\\')p=q+1;q++;}
            if(!clean_copy(p,reference,C4D89_RESOURCE_PATH_MAX)){
                result->status=C4D89_ASSET_BAD_PATH;return C4D89_OK;
            }
            result->legacy_absolute_reference=1u;
        }else{
            result->status=C4D89_ASSET_BAD_PATH;return C4D89_OK;
        }
    }
    refbase=base_name(reference);
    if(!*refbase){result->status=C4D89_ASSET_BAD_PATH;return C4D89_OK;}
    /* An index is an explicit list controlled by the caller; it can be built
       by any engine's VFS and need not correspond to a physical filesystem. */
    for(i=0u;i<index->count;i++){
        if(!index->get(index->user,i,candidate,C4D89_RESOURCE_PATH_MAX))
            return C4D89_ERR_SINK;
        {
            char validated[C4D89_RESOURCE_PATH_MAX];
            if(!clean_copy(candidate,validated,C4D89_RESOURCE_PATH_MAX))continue;
            if(!equal_ascii(base_name(validated),refbase))continue;
            level=2u;
            if(same_folder(scene,validated,1))level=0u;
            else if(same_folder(scene,validated,0))level=1u;
            if(counts[level]++==0u){
                idx[level]=i;
                strcpy(matched_path[level],validated);
            }
            matched++;
        }
    }
    result->same_folder_matches=counts[0]+counts[1];
    result->package_matches=matched;
    for(level=0u;level<3u;level++){
        if(counts[level]){
            if(counts[level]!=1u){result->status=C4D89_ASSET_AMBIGUOUS;return C4D89_OK;}
            result->status=level==0u?C4D89_ASSET_SCENE_TEX:
                (level==1u?C4D89_ASSET_SCENE_DIR:C4D89_ASSET_UNIQUE_PACKAGE);
            result->matched_index=idx[level];
            strcpy(result->path,matched_path[level]);
            return C4D89_OK;
        }
    }
    result->status=C4D89_ASSET_NONE;
    return C4D89_OK;
}
