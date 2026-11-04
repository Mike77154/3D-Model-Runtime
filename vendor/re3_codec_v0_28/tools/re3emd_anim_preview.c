/* C89, no heap, integer/fixed-point animated EMD/TIM preview.
 * v0.16: stable camera across a clip; root motion switch, validated skinning.
 * Not a 1:1 emulation of the original Resident Evil 3 renderer. */
#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define W 480
#define H 540
#define MAX_SAMPLE_KEYS 512
static re3c_model model; static re3c_tim_vram tex; static re3c_u8 image[W*H*3u]; static int depth[W*H];
static int vx[RE3C_MAX_VERTICES],vy[RE3C_MAX_VERTICES],vz[RE3C_MAX_VERTICES],sx[RE3C_MAX_VERTICES],sy[RE3C_MAX_VERTICES];
static re3c_mat34fx mats[RE3C_MAX_PARTS]; static int mode=1,textured=1,include_optional=0; static re3c_u32 morph_frame=RE3C_MORPH_REST; static int experimental_map=0; static re3c_u32 root_mode=RE3C_ROOT_MODEL_SPACE;
/* Explicit, opt-in examples; the original engine's retarget rules remain unverified. */
/* External diagnostic timeline, NOT inferred from Capcom EDD/RDT semantics.
 * Lines: KEY PART show | KEY PART hide | KEY PART morph FRAME_OR_rest
 * Each event is applied at and after KEY, until overridden. */
#define MAX_EVENTS 512u
#define EV_SHOW 1u
#define EV_HIDE 2u
#define EV_MORPH 3u
typedef struct re3c_preview_event_s {re3c_u32 key,part,frame;re3c_u8 kind;} re3c_preview_event;
static re3c_preview_event events[MAX_EVENTS];
static re3c_u32 event_count=0u;
static int events_loaded=0;
static int morph_center_mode=0; /* 0 native, 1 recenter, 2 delta */
/* v0.23: explicit PC Q12 morph blending demo. Opt-in and no claims of runtime timing. */
static int pc_blend_mode=0; /* 0 off, 1 constant, 2 pulse over clip keys */
static re3c_u32 pc_blend_channel=0u,pc_blend_weight=0u,pc_blend_clip_keys=1u,pc_blend_current_key=0u;
static re3c_u32 blend_weight_at_key(re3c_u32 key){
 re3c_u32 half,den;
 if(pc_blend_mode==1)return pc_blend_weight;
 if(pc_blend_mode!=2||pc_blend_clip_keys<2u)return 0u;
 den=pc_blend_clip_keys-1u;
 half=den/2u;
 if(half==0u)return 0u;
 /* Piecewise linear pulse 0 -> 4096 -> 0. Products use 32-bit integer arithmetic. */
 if(key<=half)return (key*4096u)/half;
 if(den==half)return 0u;
 return ((den-key)*4096u)/(den-half);
}

static re3c_vec3fx morph_offsets[RE3C_MAX_PARTS];
static re3c_u32 morph_cached_frames[RE3C_MAX_PARTS];
static re3c_u8 morph_cached[RE3C_MAX_PARTS];
static re3c_visual_state visual_state;
static int load_events(const char *path,re3c_u32 clip_keys){
 FILE *fp; char line[256]; unsigned long key,part;char op[16],value[32],extra;re3c_u32 lineno=0u;re3c_visual_state tmp;
 int r;
 fp=fopen(path,"rb");if(!fp){fprintf(stderr,"event file not found: %s\n",path);return 0;}
 r=re3c_visual_state_init(&model,&tmp,include_optional);
 if(r!=RE3C_OK){fclose(fp);return 0;}
 while(fgets(line,sizeof(line),fp)){
  int fields;char *q=line;re3c_preview_event ev;
  lineno++;
  while(*q==' '||*q=='\t')q++;
  if(!*q||*q=='#'||*q=='\n'||*q=='\r')continue;
  if(strchr(line,'\n')==NULL&&!feof(fp)){fprintf(stderr,"event line too long %u\n",(unsigned)lineno);fclose(fp);return 0;}
  value[0]='\0';extra='\0';
  fields=sscanf(q," %lu %lu %15s %31s %c",&key,&part,op,value,&extra);
  if(fields<3||fields>4||key>=clip_keys||part>=model.body.part_count||event_count>=MAX_EVENTS){fprintf(stderr,"bad event line %u\n",(unsigned)lineno);fclose(fp);return 0;}
  if(event_count>0u&&key<events[event_count-1u].key){fprintf(stderr,"unsorted key line %u\n",(unsigned)lineno);fclose(fp);return 0;}
  ev.key=(re3c_u32)key;ev.part=(re3c_u32)part;ev.frame=RE3C_MORPH_REST;
  if(strcmp(op,"show")==0&&fields==3){ev.kind=EV_SHOW;r=re3c_visual_state_set_visibility(&model,&tmp,ev.part,1);}
  else if(strcmp(op,"hide")==0&&fields==3){ev.kind=EV_HIDE;r=re3c_visual_state_set_visibility(&model,&tmp,ev.part,0);}
  else if(strcmp(op,"morph")==0&&fields==4){
   char *endp;unsigned long v;
   ev.kind=EV_MORPH;
   if(strcmp(value,"rest")!=0){
    if(value[0]=='-'){fprintf(stderr,"negative morph frame line %u\n",(unsigned)lineno);fclose(fp);return 0;}
    v=strtoul(value,&endp,10);
    if(!value[0]||*endp!='\0'||v>=0xFFFFFFFFul){fprintf(stderr,"bad morph frame line %u\n",(unsigned)lineno);fclose(fp);return 0;}
    ev.frame=(re3c_u32)v;
   }
   r=re3c_visual_state_set_morph(&model,&tmp,ev.part,ev.frame);
  }else{fprintf(stderr,"unknown event line %u\n",(unsigned)lineno);fclose(fp);return 0;}
  if(r!=RE3C_OK){fprintf(stderr,"invalid event for part %u at line %u: %s\n",(unsigned)ev.part,(unsigned)lineno,re3c_result_string(r));fclose(fp);return 0;}
  events[event_count++]=ev;
 }
 if(ferror(fp)){fclose(fp);return 0;}
 fclose(fp);events_loaded=1;
 printf("loaded %u explicit state events from %s\n",(unsigned)event_count,path);
 return 1;
}
static int state_at_key(re3c_u32 key){
 re3c_u32 i;int r;
 memset(morph_cached,0,sizeof(morph_cached));
 if(!events_loaded)return RE3C_OK;
 r=re3c_visual_state_init(&model,&visual_state,include_optional);
 if(r!=RE3C_OK)return r;
 for(i=0u;i<event_count;i++){
  const re3c_preview_event *e=&events[i];
  if(e->key>key)break;
  if(e->kind==EV_MORPH)r=re3c_visual_state_set_morph(&model,&visual_state,e->part,e->frame);
  else r=re3c_visual_state_set_visibility(&model,&visual_state,e->part,e->kind==EV_SHOW?1:0);
  if(r!=RE3C_OK)return r;
 }
 return RE3C_OK;
}
/* A centroid translation for each part/frame is cached per rendered key. */
static int preview_vertex(re3c_u32 active,re3c_u32 part,re3c_u32 vertex,re3c_vec3fx*out){
 re3c_u32 frame=events_loaded?visual_state.morph_frame[part]:morph_frame;
 if(pc_blend_mode){
  re3c_u32 ti,n,i;re3c_u16 weights[256];int r;
  r=re3c_morph_track_for_part(&model,part,&ti);
  if(r==RE3C_ERR_NOT_PRESENT)return re3c_animated_body_vertex(&model,active,mats,part,vertex,out);
  if(r!=RE3C_OK)return r;
  n=(re3c_u32)model.binary01_morph.tracks[ti].frame_count-1u;
  if(n>256u)return RE3C_ERR_OVERFLOW;
  for(i=0u;i<n;i++)weights[i]=0u;
  if(pc_blend_channel<n)weights[pc_blend_channel]=(re3c_u16)blend_weight_at_key(pc_blend_current_key);
  return re3c_animated_body_vertex_morph_blend(&model,active,mats,part,vertex,weights,n,out);
 }
 if(morph_center_mode==2 && frame!=RE3C_MORPH_REST){
  re3c_u32 ti;int r=re3c_morph_track_for_part(&model,part,&ti);
  if(r==RE3C_ERR_NOT_PRESENT)return re3c_animated_body_vertex(&model,active,mats,part,vertex,out);
  if(r!=RE3C_OK)return r;
  return re3c_animated_body_vertex_morph_delta(&model,active,mats,part,vertex,frame,out);
 }
 if(!morph_center_mode||frame==RE3C_MORPH_REST){
  if(events_loaded)return re3c_animated_body_vertex_state(&model,active,mats,&visual_state,part,vertex,out);
  return re3c_animated_body_vertex_morph(&model,active,mats,part,vertex,frame,out);
 }
 {re3c_u32 ti;int r=re3c_morph_track_for_part(&model,part,&ti);
  if(r==RE3C_ERR_NOT_PRESENT)return re3c_animated_body_vertex(&model,active,mats,part,vertex,out);
  if(r!=RE3C_OK)return r;
  if(!morph_cached[part]||morph_cached_frames[part]!=frame){
   r=re3c_morph_center_translation(&model,part,frame,&morph_offsets[part]);
   if(r!=RE3C_OK)return r;
   morph_cached[part]=1u;morph_cached_frames[part]=frame;
  }
  return re3c_animated_body_vertex_morph_offset(&model,active,mats,part,vertex,frame,&morph_offsets[part],out);
 }
}
static int decoded_pose(re3c_u32 bank,re3c_u32 clip,re3c_u32 key,re3c_pose *pose){
 re3c_pose src; int r;
 r=re3c_decode_pose_by_key(&model,bank,clip,key,&src);if(r!=RE3C_OK)return r;
 if(!experimental_map){*pose=src;return RE3C_OK;}
 if(experimental_map==1)return re3c_retarget_zombie_damage_15_to_11(&model,&src,pose);
 if(experimental_map==2)return re3c_retarget_nemesis_damage_experimental(&model,&src,pose);
 if(experimental_map==3)return re3c_retarget_gravedigger_damage_experimental(&model,&src,pose);
 if(experimental_map==4){
  re3c_pose base;
  r=re3c_decode_pose_by_key(&model,RE3C_BANK_A,0u,0u,&base);
  if(r!=RE3C_OK)return r;
  return re3c_damage_overlay_prefix(&model,&base,&src,pose);
 }
 return RE3C_ERR_BAD_ARGUMENT;
}
static re3c_u32 active_bank(re3c_u32 bank){return experimental_map?RE3C_BANK_A:bank;}
static int visible_part(const re3c_mesh_part*p){ if(events_loaded)return visual_state.visible[(re3c_u32)(p-model.body.parts)]!=0u; if(!re3c_part_should_render_default(p))return 0; if(!include_optional&&(p->part_class==RE3C_PART_EXTRA_UNBOUND||p->part_class==RE3C_PART_DYNAMIC_MORPH))return 0; return 1; }
static void put16(FILE*f,re3c_u16 v){fputc(v&255u,f);fputc((v>>8)&255u,f);} static void put32(FILE*f,re3c_u32 v){put16(f,(re3c_u16)(v&65535u));put16(f,(re3c_u16)(v>>16));}
static int bmp(const char*p){FILE*f;int y;re3c_u32 row=(re3c_u32)W*3u,size=54u+row*H;f=fopen(p,"wb");if(!f)return 0;fputc('B',f);fputc('M',f);put32(f,size);put32(f,0u);put32(f,54u);put32(f,40u);put32(f,W);put32(f,H);put16(f,1u);put16(f,24u);put32(f,0u);put32(f,row*H);put32(f,2835u);put32(f,2835u);put32(f,0u);put32(f,0u);for(y=H-1;y>=0;y--)fwrite(image+(size_t)y*W*3u,1u,(size_t)W*3u,f);fclose(f);return 1;}
static int proj(const re3c_vec3fx*v,int view,int*x,int*y,int*z){int a=re3c_fx_to_int(v->x),b=re3c_fx_to_int(v->y),c=re3c_fx_to_int(v->z);if(view==0){*x=a;*z=c;}else if(view==2){*x=-c;*z=a;}else if(view==3){*x=c;*z=-a;}else{*x=(a*2896+c*2896)/4096;*z=(c*2896-a*2896)/4096;}*y=b;return 1;}
static int edge(int ax,int ay,int bx,int by,int cx,int cy){return (cx-ax)*(by-ay)-(cy-ay)*(bx-ax);} 
static void pixel(int x,int y,int z,re3c_u32 color){size_t off=(size_t)y*W+(size_t)x;if(z<=depth[off])return;if(!(color&0xFF000000u))return;depth[off]=z;image[off*3u]=(re3c_u8)color;image[off*3u+1u]=(re3c_u8)(color>>8);image[off*3u+2u]=(re3c_u8)(color>>16);} 
static void triangle(const re3c_face*f,unsigned part,unsigned i0,unsigned i1,unsigned i2){int x0,y0,x1,y1,x2,y2,z0,z1,z2,area,sign,minx,maxx,miny,maxy,x,y;unsigned v0=f->vertex[i0],v1=f->vertex[i1],v2=f->vertex[i2];re3c_u32 solid; if(v0>=model.body.vertex_count||v1>=model.body.vertex_count||v2>=model.body.vertex_count)return; x0=sx[v0];y0=sy[v0];x1=sx[v1];y1=sy[v1];x2=sx[v2];y2=sy[v2];z0=vz[v0]/64;z1=vz[v1]/64;z2=vz[v2]/64; area=edge(x0,y0,x1,y1,x2,y2); if(area==0)return; sign=area<0?-1:1; if(area<0)area=-area; minx=x0<x1?x0:x1; if(x2<minx)minx=x2; maxx=x0>x1?x0:x1; if(x2>maxx)maxx=x2; miny=y0<y1?y0:y1; if(y2<miny)miny=y2; maxy=y0>y1?y0:y1; if(y2>maxy)maxy=y2; if(minx<0)minx=0; if(miny<0)miny=0; if(maxx>=W)maxx=W-1; if(maxy>=H)maxy=H-1; solid=0xFF000000u|((re3c_u32)(70u+(part*53u)%150u)<<16)|((re3c_u32)(75u+(part*83u)%145u)<<8)|(re3c_u32)(80u+(part*39u)%140u);
 for(y=miny;y<=maxy;y++)for(x=minx;x<=maxx;x++){ int a=edge(x1,y1,x2,y2,x,y)*sign,b=edge(x2,y2,x0,y0,x,y)*sign,c=edge(x0,y0,x1,y1,x,y)*sign,z,u,v; re3c_u32 color=solid; if(a<0||b<0||c<0)continue; z=(a*z0+b*z1+c*z2)/area; if(textured){ int rc; u=(a*f->uv[i0].u+b*f->uv[i1].u+c*f->uv[i2].u)/area; v=(a*f->uv[i0].v+b*f->uv[i1].v+c*f->uv[i2].v)/area; if(u<0)u=0; if(v<0)v=0; if(u>255)u=255; if(v>255)v=255; rc=re3c_tim_sample_argb(&tex,f->page,f->clut,(re3c_u8)u,(re3c_u8)v,&color); if(rc!=RE3C_OK)color=0xFFFF00FFu; } pixel(x,y,z,color);} }
static int sample_bounds(re3c_u32 bank,re3c_u32 clip,re3c_u32 key,int*gx0,int*gx1,int*gy0,int*gy1){ re3c_pose pose; re3c_u32 p,k; int first=1,r; pc_blend_current_key=key; r=state_at_key(key);if(r!=RE3C_OK)return r; r=decoded_pose(bank,clip,key,&pose); if(r!=RE3C_OK)return r; r=re3c_build_joint_matrices(&model.banks[active_bank(bank)],&pose,root_mode,mats); if(r!=RE3C_OK)return r; for(p=0;p<model.body.part_count;p++){ const re3c_mesh_part*part=&model.body.parts[p]; if(!visible_part(part))continue; for(k=0;k<part->vertex_count;k++){ re3c_vec3fx v; int px,py,pz; unsigned i=part->first_vertex+k; r=preview_vertex(active_bank(bank),p,k,&v); if(r!=RE3C_OK)return r; proj(&v,mode,&px,&py,&pz); vx[i]=px; vy[i]=py; vz[i]=pz; if(first){*gx0=*gx1=px; *gy0=*gy1=py; first=0;} else { if(px<*gx0)*gx0=px; if(px>*gx1)*gx1=px; if(py<*gy0)*gy0=py; if(py>*gy1)*gy1=py; } } } return RE3C_OK; }
static int render_frame(re3c_u32 bank,re3c_u32 clip,re3c_u32 key,int xmin,int xmax,int ymin,int ymax,const char*outpath){ re3c_pose pose; re3c_u32 p,k; int r,range_x,range_y,scale,span,hspan; size_t off; pc_blend_current_key=key; r=state_at_key(key);if(r!=RE3C_OK)return r; r=decoded_pose(bank,clip,key,&pose); if(r!=RE3C_OK)return r; r=re3c_build_joint_matrices(&model.banks[active_bank(bank)],&pose,root_mode,mats); if(r!=RE3C_OK)return r; for(p=0;p<model.body.part_count;p++){ const re3c_mesh_part*part=&model.body.parts[p]; if(!visible_part(part))continue; for(k=0;k<part->vertex_count;k++){ re3c_vec3fx v; unsigned i=part->first_vertex+k; r=preview_vertex(active_bank(bank),p,k,&v); if(r!=RE3C_OK)return r; proj(&v,mode,&vx[i],&vy[i],&vz[i]); } }
 range_x=xmax-xmin; range_y=ymax-ymin; if(range_x<1)range_x=1; if(range_y<1)range_y=1; scale=((W-60)*4096)/range_x; if(((H-60)*4096)/range_y<scale)scale=((H-60)*4096)/range_y; if(scale<1)scale=1; span=range_x*scale/4096; hspan=range_y*scale/4096;
 for(p=0;p<model.body.part_count;p++){ const re3c_mesh_part*part=&model.body.parts[p]; if(!visible_part(part))continue; for(k=0;k<part->vertex_count;k++){ unsigned i=part->first_vertex+k; sx[i]=(W-span)/2+(vx[i]-xmin)*scale/4096; sy[i]=(H-hspan)/2+(vy[i]-ymin)*scale/4096; } }
 for(off=0;off<(size_t)W*H;off++){ image[off*3u]=238; image[off*3u+1u]=242; image[off*3u+2u]=246; depth[off]=-2000000000; }
 for(p=0;p<model.body.part_count;p++){ const re3c_mesh_part*part=&model.body.parts[p]; if(!visible_part(part))continue; for(k=0;k<part->face_count;k++){ const re3c_face*f=&model.body.faces[part->first_face+k]; if(f->vertex_count==3u)triangle(f,p,0u,1u,2u); else if(f->vertex_count==4u){ triangle(f,p,0u,1u,3u); triangle(f,p,0u,3u,2u);} } }
 return bmp(outpath)?RE3C_OK:RE3C_ERR_IO; }
int main(int argc,char**argv){ re3c_u32 bank,clip,max_frames,count,step,key_indices[MAX_SAMPLE_KEYS],sample_count=0,i; int r,x0=0,x1=0,y0=0,y1=0,gx0=0,gx1=0,gy0=0,gy1=0,first=1; char out[1024]; if(argc<6||argc>16){fprintf(stderr,"usage: %s file.EMD file.TIM outdir bank clip [max_frames] [front|quarter|profile|back] [solid|texture] [model|frame] [main|all] [rest|morph:N] [no-map|zombie15to11|nemesis15to16|grave15to17|damage-prefix] [state_events.txt|-] [native|recenter|delta] [pc:INDEX:WEIGHT|pc-pulse:INDEX]\n",argv[0]);return 2;} bank=(re3c_u32)atoi(argv[4]); clip=(re3c_u32)atoi(argv[5]); max_frames=(argc>=7)?(re3c_u32)atoi(argv[6]):12u; if(argc>=8){ if(strcmp(argv[7],"front")==0)mode=2; else if(strcmp(argv[7],"profile")==0)mode=0; else if(strcmp(argv[7],"back")==0)mode=3; } if(argc>=9&&strcmp(argv[8],"solid")==0)textured=0;
 if(argc>=10){ if(strcmp(argv[9],"frame")==0)root_mode=RE3C_ROOT_FRAME_SPACE; else if(strcmp(argv[9],"model")!=0){fprintf(stderr,"invalid root mode: model|frame\n");return 2;}}
 if(argc>=11){if(strcmp(argv[10],"all")==0)include_optional=1;else if(strcmp(argv[10],"main")!=0){fprintf(stderr,"invalid part visibility: main|all\n");return 2;}}
 if(argc>=12){if(strcmp(argv[11],"rest")==0)morph_frame=RE3C_MORPH_REST;
  else if(strncmp(argv[11],"morph:",6)==0){char*endp;unsigned long x=strtoul(argv[11]+6,&endp,10);if(*endp!='\0'||x>65535ul){fprintf(stderr,"invalid morph frame\n");return 2;}morph_frame=(re3c_u32)x;}
  else{fprintf(stderr,"invalid morph option\n");return 2;}}
 if(argc>=13){if(strcmp(argv[12],"zombie15to11")==0)experimental_map=1;
  else if(strcmp(argv[12],"nemesis15to16")==0)experimental_map=2;
  else if(strcmp(argv[12],"grave15to17")==0)experimental_map=3;
  else if(strcmp(argv[12],"damage-prefix")==0)experimental_map=4;
  else if(strcmp(argv[12],"no-map")!=0){fprintf(stderr,"invalid map option\n");return 2;}}
 if(max_frames==0u||max_frames>MAX_SAMPLE_KEYS){fprintf(stderr,"max_frames must be 1..%u\n",(unsigned)MAX_SAMPLE_KEYS);return 2;}
 if(strlen(argv[3])>sizeof(out)-32u){fprintf(stderr,"output path too long\n");return 2;}
 re3c_model_init(&model); r=re3c_open_file(&model,argv[1]); if(r!=RE3C_OK){fprintf(stderr,"EMD: %s\n",re3c_result_string(r));return 1;} if(textured){ re3c_tim_init(&tex); r=re3c_tim_load_file(&tex,argv[2]); if(r!=RE3C_OK){fprintf(stderr,"TIM: %s\n",re3c_result_string(r));return 1;} }
 if(bank>=RE3C_BANK_COUNT || !model.banks[bank].present || clip>=model.banks[bank].clip_count){ fprintf(stderr,"invalid bank/clip\n"); return 1; }
 if(!experimental_map && model.banks[bank].joint_count!=model.banks[RE3C_BANK_A].joint_count){fprintf(stderr,"bank %u has %u joints vs normal %u: animation-body binding not proven; refusing unsafe rendering\n",(unsigned)bank,(unsigned)model.banks[bank].joint_count,(unsigned)model.banks[RE3C_BANK_A].joint_count);return 1;}
 if(experimental_map){if(bank!=RE3C_BANK_DAMAGE){fprintf(stderr,"experimental mapping only allowed for damage bank\n");return 2;}
  if(model.banks[bank].joint_count!=15u||(experimental_map==4 ? model.banks[RE3C_BANK_A].joint_count<model.banks[bank].joint_count : model.banks[RE3C_BANK_A].joint_count!=(experimental_map==1?11u:(experimental_map==2?16u:17u)))){fprintf(stderr,"unsupported mapping dimensions\n");return 1;}}
 if(morph_frame!=RE3C_MORPH_REST){re3c_u32 p;for(p=0;p<model.body.part_count;p++){re3c_u32 ti;if(!visible_part(&model.body.parts[p]))continue;
  if(re3c_morph_track_for_part(&model,p,&ti)==RE3C_OK && morph_frame>=model.binary01_morph.tracks[ti].frame_count){fprintf(stderr,"morph frame %u exceeds track on part %u\n",(unsigned)morph_frame,(unsigned)p);return 1;}}}
 if(model.body.part_count==0u){fprintf(stderr,"empty body geometry\n");return 1;}
 if(argc>=14&&strcmp(argv[13],"-")!=0){if(morph_frame!=RE3C_MORPH_REST){fprintf(stderr,"state timeline requires rest (events set morph per-part)\n");return 2;}
  if(!load_events(argv[13],model.banks[bank].clips[clip].key_count))return 1;}

 if(argc>=15){if(strcmp(argv[14],"recenter")==0)morph_center_mode=1;else if(strcmp(argv[14],"delta")==0)morph_center_mode=2;else if(strcmp(argv[14],"native")!=0){fprintf(stderr,"morph space: native|recenter|delta\n");return 2;}}
 if(argc>=16){
  unsigned int ch=0u,w=0u;char extra='\0';
  if(sscanf(argv[15],"pc:%u:%u%c",&ch,&w,&extra)==2 && w<=4096u && ch<256u){pc_blend_mode=1;pc_blend_channel=ch;pc_blend_weight=w;}
  else if(sscanf(argv[15],"pc-pulse:%u%c",&ch,&extra)==1 && ch<256u){pc_blend_mode=2;pc_blend_channel=ch;}
  else{fprintf(stderr,"invalid PC Q12 morph spec: pc:INDEX:WEIGHT or pc-pulse:INDEX\n");return 2;}
  if(morph_frame!=RE3C_MORPH_REST||events_loaded){fprintf(stderr,"PC morph comparison requires rest and no external timeline\n");return 2;}
 }

 count=model.banks[bank].clips[clip].key_count;pc_blend_clip_keys=count; if(count==0u){fprintf(stderr,"empty clip\n");return 1;} step=(count+max_frames-1u)/max_frames; if(step<1u)step=1u; for(i=0;i<count && sample_count<MAX_SAMPLE_KEYS;i+=step) key_indices[sample_count++]=i; if(sample_count==0u) key_indices[sample_count++]=0u;
 if(events_loaded){re3c_u32 e,j;
  for(e=0u;e<event_count;e++){
   int found=0;
   for(j=0u;j<sample_count;j++)if(key_indices[j]==events[e].key){found=1;break;}
   if(!found){if(sample_count>=MAX_SAMPLE_KEYS){fprintf(stderr,"too many sample/event keys\n");return 1;}key_indices[sample_count++]=events[e].key;}
  }
  for(e=1u;e<sample_count;e++){
   re3c_u32 key=key_indices[e];j=e;
   while(j>0u&&key_indices[j-1u]>key){key_indices[j]=key_indices[j-1u];j--;}
   key_indices[j]=key;
  }
 }

 /* Constant-weight comparisons share camera bounds across 0..4096, not an independently zoomed camera. */
 for(i=0;i<sample_count;i++){
  re3c_u32 sweep,iterations=pc_blend_mode==1?5u:1u, saved_weight=pc_blend_weight;
  for(sweep=0u;sweep<iterations;sweep++){
   if(pc_blend_mode==1)pc_blend_weight=sweep*1024u;
   r=sample_bounds(bank,clip,key_indices[i],&x0,&x1,&y0,&y1);
   if(r!=RE3C_OK){fprintf(stderr,"bounds fail key=%u: %s\n",(unsigned)key_indices[i],re3c_result_string(r));return 1;}
   if(first){gx0=x0;gx1=x1;gy0=y0;gy1=y1;first=0;}
   else {if(x0<gx0)gx0=x0;if(x1>gx1)gx1=x1;if(y0<gy0)gy0=y0;if(y1>gy1)gy1=y1;}
  }
  pc_blend_weight=saved_weight;
 }
 for(i=0;i<sample_count;i++){ sprintf(out,"%s/frame_%03u.bmp",argv[3],(unsigned)key_indices[i]); r=render_frame(bank,clip,key_indices[i],gx0,gx1,gy0,gy1,out); if(r!=RE3C_OK){fprintf(stderr,"render fail key=%u: %s\n",(unsigned)key_indices[i],re3c_result_string(r)); return 1;} printf("wrote %s\n",out); }
 re3c_model_close(&model); return 0; }
