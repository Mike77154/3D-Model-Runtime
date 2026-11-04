#include "c4d89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_CATALOG 1024u
static char entries[MAX_CATALOG][C4D89_RESOURCE_PATH_MAX];
static unsigned int count=0u;
static C4D89_BitmapCatalog bitmaps;
static int get_name(void *unused,unsigned int index,char *out,unsigned int cap)
{
    (void)unused;
    if(index>=count||strlen(entries[index])+1u>cap)return 0;
    strcpy(out,entries[index]);return 1;
}
int main(int argc,char **argv)
{
    FILE *manifest;char line[C4D89_RESOURCE_PATH_MAX+3u];unsigned int i;
    C4D89_ResourceIndex index;C4D89_ResourceMatch result;int rc;
    if(argc!=4){fputs("usage: c4d89_asset_audit native.c4d package/scene.c4d resource_manifest.txt\n",stderr);return 2;}
    manifest=fopen(argv[3],"rb");if(!manifest)return 1;
    while(fgets(line,sizeof(line),manifest)){
        size_t len=strlen(line);
        if(len==sizeof(line)-1u&&line[len-1u]!='\n'){fclose(manifest);return 1;}
        while(len&&(line[len-1u]=='\r'||line[len-1u]=='\n'))line[--len]='\0';
        if(len==0u||line[0]=='#')continue;
        if(len>=C4D89_RESOURCE_PATH_MAX||count>=MAX_CATALOG){fclose(manifest);return 1;}
        strcpy(entries[count++],line);
    }
    fclose(manifest);
    index.get=get_name;index.user=NULL;index.count=count;
    rc=c4d89_scan_material_bitmaps(argv[1],&bitmaps);
    if(rc){fprintf(stderr,"scan error %d\n",rc);return 1;}
    for(i=0u;i<bitmaps.count;i++){
        rc=c4d89_match_bitmap_asset(argv[2],bitmaps.paths[i].name,&index,&result);
        if(rc){fprintf(stderr,"match error %d\n",rc);return 1;}
        printf("%u\t%s\t%s\t%u\t%u\t%s\t%u\n",i,
            bitmaps.paths[i].name,argv[2],result.status,
            result.package_matches,result.path,result.legacy_absolute_reference);
    }
    return 0;
}
