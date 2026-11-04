#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "vendor_dbo/dbo_vendor.h"
#include "vendor_fragmotion/frag_probe.h"
#include "vendor_fragmotion/ugh_forensics.h"
#include "vendor_fragmotion/arg_vendor.h"
#include "vendor_blade3d/xmdl_vendor.h"
#include "vendor_blade3d/xir_vendor.h"
#include "vendor_directx_x/x_vendor.h"
#include "vendor_gamestudio/gs_dispatch.h"
#include "vendor_gamestudio/a6_a8/mdl7_rig.h"
static const char *ext_of(const char *name) {
 const char *p=strrchr(name,'.');return p?p+1:"";
}
static int equal_ext(const char *a,const char *b) {
 while(*a && *b){if(tolower((unsigned char)*a)!=tolower((unsigned char)*b))return 0;a++;b++;}
 return !*a && !*b;
}
int main(int argc,char **argv) {
 FILE *fp,*obj=NULL,*rig=NULL,*mtl=NULL;DBO_Stats s;XIR_Stats xi;ARG_Stats as;X_Stats xs;GSStats gs;const char *ext;int ok,bake=0,skinmode=0;unsigned long mdlframe=0UL,blend=0UL;int blend_seen=0,blend_rot=0,face_normals=0,smooth_normals=0,arg_clip_seen=0,arg_frame_seen=0; ARG_PoseSelection argpose;
 memset(&argpose,0,sizeof(argpose));
 if(argc<2 || argc>12) {fprintf(stderr,"Usage: fraginspect <model.dbo|model.ugh|model.arg|model.xmdl|model.x|model.mdl|test_bridge.xir> [out.obj] [meta.tsv] [out.mtl ARG-only] [--bake|--frame=N] [--skin-raw|--skin-pivot|--skin-hierarchy|--skin-invbind] [--blend-next=1..65536] [--blend-rot] [--face-normals|--smooth-normals] [--arg-clip=N --arg-frame=N (ARG TSV only)]\n");return 2;}
 ext=ext_of(argv[1]);
 while(argc>=3){
  const char *flag=argv[argc-1];
  if(!strncmp(flag,"--arg-clip=",11) || !strncmp(flag,"--arg-frame=",12)){
   int isclip=!strncmp(flag,"--arg-clip=",11);
   const char *v=flag+(isclip?11:12);char *tail;unsigned long num;
   if(!equal_ext(ext,"arg")||!*v||!isdigit((unsigned char)*v)){
    fputs("ARG pose options require .arg and decimal values\n",stderr);return 2;
   }
   num=strtoul(v,&tail,10);
   if(*tail||num>2147483647UL){fputs("ARG pose value out of range\n",stderr);return 2;}
   if(isclip){if(arg_clip_seen)return 2;arg_clip_seen=1;argpose.clip=num;}
   else{if(arg_frame_seen)return 2;arg_frame_seen=1;argpose.frame=num;}
   argc--;continue;
  }
  if(!strncmp(flag,"--frame=",8)){
   const char *p=flag+8;char *tail;
   if(!equal_ext(ext,"mdl")||!*p){fprintf(stderr,"--frame=N requires .mdl and an index\n");return 2;}
   mdlframe=strtoul(p,&tail,10);
   if(*tail){fprintf(stderr,"Invalid frame index\n");return 2;}
   argc--;continue;
  }
  if(!strncmp(flag,"--blend-next=",13)){
   const char *p=flag+13;char *tail;unsigned long v;
   if(!equal_ext(ext,"mdl")||blend_seen||!(*p>='0'&&*p<='9')){
    fputs("--blend-next requires a single decimal Q16 value for MDL7\n",stderr);return 2;
   }
   v=strtoul(p,&tail,10);
   if(*tail || v<1UL || v>65536UL){
    fputs("--blend-next must be 1..65536\n",stderr);return 2;
   }
   blend=v;blend_seen=1;argc--;continue;
  }
  if(!strcmp(flag,"--face-normals")){
   if(!equal_ext(ext,"mdl")||face_normals||smooth_normals){fputs("--face-normals supports MDL7 once only\n",stderr);return 2;}
   face_normals=1;argc--;continue;
  }
  if(!strcmp(flag,"--smooth-normals")){
   if(!equal_ext(ext,"mdl")||face_normals||smooth_normals){fputs("--smooth-normals conflicts with --face-normals and requires MDL7\n",stderr);return 2;}
   smooth_normals=1;argc--;continue;
  }
  if(!strcmp(flag,"--blend-rot")){
   if(!equal_ext(ext,"mdl")||blend_rot){fputs("--blend-rot may appear only once for MDL7\n",stderr);return 2;}
   blend_rot=1;argc--;continue;
  }
  if(!strcmp(flag,"--skin-raw") || !strcmp(flag,"--skin-pivot") || !strcmp(flag,"--skin-hierarchy") || !strcmp(flag,"--skin-invbind")){
   if(!equal_ext(ext,"mdl") || skinmode){fprintf(stderr,"Only one MDL7 --skin option can be used\n");return 2;}
   skinmode=!strcmp(flag,"--skin-invbind")?4:(!strcmp(flag,"--skin-hierarchy")?3:(!strcmp(flag,"--skin-pivot")?2:1));
   argc--;continue;
  }
  if(!strcmp(flag,"--bake")){
   if(!equal_ext(ext,"x") || argc<4){fprintf(stderr,"--bake requires .x and output OBJ\n");return 2;}
   bake=1;argc--;continue;
  }
  break;
 }
 if(arg_clip_seen!=arg_frame_seen || (arg_clip_seen && argc<4)){
  fputs("ARG pose requires --arg-clip=N --arg-frame=N and metadata TSV output\n",stderr);return 2;
 }
 argpose.enabled=arg_clip_seen;
 if(argc>4 && !(argc==5 && equal_ext(ext,"arg"))){fprintf(stderr,"Too many arguments\n");return 2;}
 if(blend_rot && (!blend_seen || skinmode<2)){
  fputs("--blend-rot requires --blend-next=N and --skin-pivot or --skin-hierarchy/--skin-invbind\n",stderr);return 2;
 }
 if(argc==4 && !equal_ext(ext,"arg") && !equal_ext(ext,"x") && !equal_ext(ext,"mdl")){fprintf(stderr,"metadata TSV output is only available for ARG/X/MDL\n");return 2;}
 fp=fopen(argv[1],"rb");if(!fp){perror(argv[1]);return 2;}
 if(equal_ext(ext,"dbo")) {
  if(argc==3){obj=fopen(argv[2],"wb");if(!obj){perror(argv[2]);fclose(fp);return 2;}}
  ok=dbo_scan(fp,obj,&s,1);
  printf("DBO %s | version=%lu blocks=%lu frames=%lu meshes=%lu verts=%lu indices=%lu textures=%lu animations=%lu named_frames=%lu matrices=%lu tracks=%lu pos_keys=%lu error_at=%lu\n", ok?"OK":"INVALID",s.version,s.blocks,s.frames,s.meshes,s.vertices,s.indexes,s.textures,s.animations,s.named_frames,s.frame_matrices,s.animation_tracks,s.position_keys,s.error_offset);
 } else if(equal_ext(ext,"arg")){
  if(argc>=4){rig=fopen(argv[3],"wb");if(!rig){perror(argv[3]);fclose(fp);return 2;}}
  if(argc>=3){obj=fopen(argv[2],"wb");if(!obj){perror(argv[2]);if(rig)fclose(rig);fclose(fp);return 2;}}
  if(argc==5){mtl=fopen(argv[4],"wb");if(!mtl){perror(argv[4]);if(rig)fclose(rig);if(obj)fclose(obj);fclose(fp);return 2;}}
  ok=arg_decode_pose_ex(fp,obj,rig,mtl,mtl?argv[4]:NULL,&as,1,&argpose);
  if(argpose.enabled)printf("ARG sampled clip=%lu frame=%lu bones=%lu channels=%lu\n",argpose.clip,argpose.frame,argpose.bones_seen,argpose.channels_sampled);
  printf("ARG SUMMARY keys(r/t/s)=%lu/%lu/%lu skin_verts=%lu influences=%lu attachments=%lu\n",as.rotate_keys,as.translate_keys,as.scale_keys,as.skinned_vertices,as.weighted_influences,as.attachments);
 } else if(equal_ext(ext,"x")){
  if(argc==4){rig=fopen(argv[3],"wb");if(!rig){perror(argv[3]);fclose(fp);return 2;}}
  if(argc>=3){obj=fopen(argv[2],"wb");if(!obj){perror(argv[2]);if(rig)fclose(rig);fclose(fp);return 2;}}
  ok=x_decode_ex(fp,obj,rig,&xs,1,bake);
 } else if(equal_ext(ext,"mdl")){
  if(skinmode || blend_seen || face_normals || smooth_normals){unsigned char head[4];if(fread(head,1,4,fp)!=4||memcmp(head,"MDL7",4)!=0||fseek(fp,0L,SEEK_SET)!=0){fputs("--skin and --blend-next support MDL7 only\n",stderr);fclose(fp);return 2;}}
  if(argc==4){rig=fopen(argv[3],"wb");if(!rig){perror(argv[3]);fclose(fp);return 2;}}
  if(argc>=3){obj=fopen(argv[2],"wb");if(!obj){perror(argv[2]);if(rig)fclose(rig);fclose(fp);return 2;}}
  gs7_set_skin_mode(skinmode);
  gs7_set_blend_next((long)blend);
  gs7_set_blend_rot(blend_rot);
  gs_set_face_normals(face_normals);
  if(smooth_normals)gs_set_smooth_normals(1);
  ok=gs_dispatch(fp,obj,rig,&gs,mdlframe);
  printf("GameStudio MDL%lu %s | verts=%lu tris=%lu frames=%lu skins=%lu bones=%lu groups=%lu deformers=%lu influences=%lu matrices=%lu %s\n",gs.version,ok?"OK":"UNSUPPORTED_OR_INVALID",gs.vertices,gs.triangles,gs.frames,gs.skins,gs.bones,gs.groups,gs.deformers,gs.influences,gs.bone_matrices,gs.unsupported?"[PROBE ONLY]":"");
 } else if(equal_ext(ext,"ugh")){
  if(argc>=3){rig=fopen(argv[2],"wb");if(!rig){perror(argv[2]);fclose(fp);return 2;}}
  ok=ugh_inspect(fp,rig?rig:stdout);
 } else if(equal_ext(ext,"xmdl")){
  ok=xmdl_probe(fp);
 } else if(equal_ext(ext,"xir")){
  if(argc==3){obj=fopen(argv[2],"wb");if(!obj){perror(argv[2]);fclose(fp);return 2;}}
  ok=xir_decode(fp,obj,&xi,1);
  printf("XIR %s; records=%lu, vertices=%lu, triangles=%lu, stride=%lu, errors=%lu (INTERNAL TEST FORMAT, not Blade3D XMDL)\n",ok?"OK":"INVALID",xi.records,xi.vertices,xi.triangles,xi.stride,xi.errors);
 } else {fprintf(stderr,"Unsupported extension\n");ok=0;}
 if(obj)fclose(obj);
 if(rig)fclose(rig);
 if(mtl)fclose(mtl);
 fclose(fp);return ok?0:1;
}
