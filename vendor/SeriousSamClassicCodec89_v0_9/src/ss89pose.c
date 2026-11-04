#include "ss89skin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int usage(void){fprintf(stderr,"ss89pose mesh.bm rig.skl clip.ba [--clip N] [--frame N] [--sub N] [--lod N] [--loop] --obj output.obj [--sequence prefix --count N]\n  N for --sub: fraction of frame 0..65535\n");return 2;}
int main(int argc,char**argv){unsigned clip=0U,frame=0U,sub=0U,lod=0U,i,nframes=0U;int loop=0;const char*obj=NULL,*prefix=NULL;SS89S_Info info;
 if(argc<4)return usage();
 for(i=4U;i<(unsigned)argc;i++){
  if(!strcmp(argv[i],"--loop"))loop=1;
  else if(i+1U<(unsigned)argc&&!strcmp(argv[i],"--clip"))clip=(unsigned)strtoul(argv[++i],NULL,10);
  else if(i+1U<(unsigned)argc&&!strcmp(argv[i],"--frame"))frame=(unsigned)strtoul(argv[++i],NULL,10);
  else if(i+1U<(unsigned)argc&&!strcmp(argv[i],"--sub"))sub=(unsigned)strtoul(argv[++i],NULL,10);
  else if(i+1U<(unsigned)argc&&!strcmp(argv[i],"--lod"))lod=(unsigned)strtoul(argv[++i],NULL,10);
  else if(i+1U<(unsigned)argc&&!strcmp(argv[i],"--obj"))obj=argv[++i];
  else if(i+1U<(unsigned)argc&&!strcmp(argv[i],"--sequence"))prefix=argv[++i];
  else if(i+1U<(unsigned)argc&&!strcmp(argv[i],"--count"))nframes=(unsigned)strtoul(argv[++i],NULL,10);
  else return usage();
 }
 if(!ss89s_load_mesh(argv[1],lod)){fprintf(stderr,"MESH: %s\n",ss89s_error());return 1;}
 if(!ss89s_load_skeleton(argv[2],lod)){fprintf(stderr,"SKEL: %s\n",ss89s_error());return 1;}
 if(!ss89s_load_animation(argv[3])){fprintf(stderr,"ANIM: %s\n",ss89s_error());return 1;}
 ss89s_info(&info);
 printf("MESH vertices=%u tris=%u weights=%u / SKEL bones=%u / ANIM clips=%u\n",info.vertices,info.triangles,info.weights,info.bones,info.clips);
 for(i=0;i<info.clips;i++)printf("  clip[%u] %s frames=%u\n",i,ss89s_clip_name(i),ss89s_clip_frames(i));
 if(!ss89s_pose(clip,frame,sub,loop)){fprintf(stderr,"POSE: %s\n",ss89s_error());return 1;}
 if(obj&&!ss89s_export_obj(obj)){fprintf(stderr,"OBJ: %s\n",ss89s_error());return 1;}
 if(prefix){
   char out[1024];
   if(nframes==0U||nframes>10000U||strlen(prefix)>980U)return usage();
   for(i=0U;i<nframes;i++){
     if(!ss89s_pose(clip,frame+i,sub,loop)){fprintf(stderr,"POSE: %s\n",ss89s_error());return 1;}
     sprintf(out,"%s%04u.obj",prefix,i);
     if(!ss89s_export_obj(out)){fprintf(stderr,"OBJ: %s\n",ss89s_error());return 1;}
   }
   printf("Sequence exported: %u OBJ pose frames\n",nframes);
 }
 printf("Pose complete: clip=%u frame=%u sub=%u%s%s\n",clip,frame,sub,loop?" looping":"",obj?" OBJ saved":"");return 0;
}
