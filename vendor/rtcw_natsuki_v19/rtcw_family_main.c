#include "rtcw_family.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
    int ok=0;
    if(argc==4 && strcmp(argv[1],"index")==0)
        ok=rtcw_family_index(argv[2],argv[3]);
    else if(argc==5 && strcmp(argv[1],"bundle")==0)
        ok=rtcw_family_bundle(argv[2],argv[3],argv[4]);
    else if(argc==4 && strcmp(argv[1],"index-vfs")==0)
        ok=rtcw_family_index_vfs(argv[2],argv[3]);
    else if(argc==5 && strcmp(argv[1],"bundle-vfs")==0)
        ok=rtcw_family_bundle_vfs(argv[2],argv[3],argv[4]);
    else {
        fprintf(stderr,"Usage:\n rtcw_family index archive.pk3 report.tsv\n rtcw_family bundle archive.pk3 models/creature/body.mds NEW_output_dir\n rtcw_family index-vfs mounts.txt report.tsv\n rtcw_family bundle-vfs mounts.txt models/creature/body.mds NEW_output_dir\n");
        return 2;
    }
    if(!ok){fprintf(stderr,"RTCW family: %s\n",rtcw_family_error());return 1;}
    return 0;
}
