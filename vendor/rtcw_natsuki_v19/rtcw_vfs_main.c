/* CLI for VFS: read multi-PK3 pack lists, report winners and collisions. */
#include "rtcw_vfs.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
    int ok=0;rtcw_vfs_file f;
    if(argc<4){
        fprintf(stderr,"Usage:\n rtcw_vfs index mounts.txt report.tsv\n rtcw_vfs conflicts mounts.txt overrides.tsv\n rtcw_vfs where mounts.txt virtual/path\n rtcw_vfs extract mounts.txt virtual/path output.file\n rtcw_vfs unpack mounts.txt NEW_dir all|models|textures\n");return 2;
    }
    if(!rtcw_vfs_mounts(argv[2])){
        fprintf(stderr,"mount error: %s\n",rtcw_vfs_error());return 1;
    }
    if(argc==4&&!strcmp(argv[1],"index"))ok=rtcw_vfs_report(argv[3],0);
    else if(argc==4&&!strcmp(argv[1],"conflicts"))ok=rtcw_vfs_report(argv[3],1);
    else if(argc==4&&!strcmp(argv[1],"where")){
        ok=rtcw_vfs_lookup(argv[3],&f);
        if(ok)printf("WINNER\t%s\t%s\t%lu\t%u\n",f.entry.name,rtcw_vfs_mount_name(f.mount),f.entry.uncompressed_size,f.overrides);
    }else if(argc==5&&!strcmp(argv[1],"extract")){
        FILE *probe=fopen(argv[4],"rb");
        if(probe){fclose(probe);fprintf(stderr,"refusing to overwrite existing output\n");return 1;}
        ok=rtcw_vfs_extract(argv[3],argv[4]);
    }else if(argc==5&&!strcmp(argv[1],"unpack"))ok=rtcw_vfs_unpack(argv[3],argv[4]);
    else {fprintf(stderr,"invalid command/argument count\n");return 2;}
    if(!ok){fprintf(stderr,"VFS error: %s\n",rtcw_vfs_error());return 1;}
    return 0;
}
