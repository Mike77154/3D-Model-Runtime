/* Source-only C89 multi-part tag assembly. Plan: tag path frame skin|- per line. */
#include "rtcw_codec.h"
#include "rtcw_texrender.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
static int read_frame(const char *s,unsigned long *out){
    char *end;errno=0;*out=strtoul(s,&end,10);
    return !errno&&end!=s&&!*end&&s[0]!='-';
}
int main(int argc,char **argv){
    FILE *plan;unsigned long frame,child_frame;unsigned int count=0;
    char row[768],tag[65],filename[512],skin[192],frame_text[40],tail;
    int n,ch;
    if((argc!=8&&argc!=11)||!read_frame(argv[2],&frame)){
        fprintf(stderr,"Usage: rtcw_multiattach parent.mds frame skin|- plan.txt out.obj out.ppm report.txt [asset_root textured.ppm shaders.shader|-]\n");return 2;
    }
    plan=fopen(argv[4],"rb");if(!plan){fprintf(stderr,"cannot open assembly plan\n");return 1;}
    if(!rtcw_attach_begin(argv[1],frame,strcmp(argv[3],"-")==0?NULL:argv[3])){
        fclose(plan);fprintf(stderr,"parent: %s\n",rtcw_attach_error());return 1;
    }
    while(fgets(row,sizeof(row),plan)){
        size_t len=strlen(row);char *p=row;
        if(len && row[len-1]!='\n' && !feof(plan)){
            while((ch=fgetc(plan))!='\n'&&ch!=EOF){}
            fclose(plan);fprintf(stderr,"assembly plan line too long\n");return 1;
        }
        while(*p==' '||*p=='\t')p++;
        if(!*p||*p=='\r'||*p=='\n'||*p=='#'||(*p=='/'&&p[1]=='/'))continue;
        tail=0;
        n=sscanf(p,"%64s %511s %39s %191s %c",tag,filename,frame_text,skin,&tail);
        if(n!=4 || !read_frame(frame_text,&child_frame) || count>=30U){fclose(plan);fprintf(stderr,"bad plan line or >30 pieces\n");return 1;}
        if(!rtcw_attach_add(tag,filename,child_frame,strcmp(skin,"-")==0?NULL:skin)){
            fclose(plan);fprintf(stderr,"append %u: %s\n",count+1U,rtcw_attach_error());return 1;
        }
        count++;
    }
    if(ferror(plan)){fclose(plan);fprintf(stderr,"plan read failed\n");return 1;}
    fclose(plan);
    if(!rtcw_save_obj(argv[5])||!rtcw_save_ppm(argv[6])||!rtcw_report(argv[7])){
        fprintf(stderr,"output: %s\n",rtcw_error());return 1;
    }
    if(argc==11){
        if(!rtcw_render_textured(argv[9],argv[8],strcmp(argv[10],"-")==0?NULL:argv[10])){
            fprintf(stderr,"textured preview: %s\n",rtcw_render_error());return 1;
        }
        printf("Textured preview: %s (%u unresolved surfaces)\n",argv[9],
               rtcw_render_missing_textures());
    }
    printf("OK %u attached pieces, %lu vertices, %lu triangles\n",count,
           rtcw_current.vertex_count,rtcw_current.triangle_count);
    return 0;
}
