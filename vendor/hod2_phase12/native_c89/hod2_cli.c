#include "hod2_native.h"
#include "../inherited/c89/hod2_extra_parts.h"
#include "../inherited/c89/hod2_character_clips.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define source hod2_workspace_raw
#define decoded hod2_workspace_decoded
static unsigned char executable[24UL*1024UL*1024UL];
static unsigned char pol_copy[HOD2_FILE_CAP];
static Hod2Node nodes[HOD2_MAX_NODES];
static unsigned char seen_models[32768];
static unsigned long uv_model[32768];
static Hod2PE pe;
static unsigned long palette[16];
static const char *errmsg="bad file or malformed data";
static size_t file_size;
static int load_data(const char *p){size_t n;
 if(!hod2_read_file(p,source,sizeof(source),&n))return 0;
 return hod2_file_decode(source,n,decoded,sizeof(decoded),&file_size);
}
/* Accept a real EXE or the H2TB data-only table archive. */
static int load_pe(const char *name,int profile){size_t n;
 return hod2_read_file(name,executable,sizeof(executable),&n)&&hod2_pe_init(&pe,executable,n,profile);
}
static int number(const char *s,unsigned long *out){char *e;unsigned long v;
 if(!s||!*s||*s=='-')return 0;
 v=strtoul(s,&e,0);if(*e)return 0;*out=v;return 1;
}
static int parse_version(const char *s,int *v){if(!strcmp(s,"1999")||!strcmp(s,"y99")){*v=0;return 1;}if(!strcmp(s,"2000")||!strcmp(s,"y2000")){*v=1;return 1;}return 0;}
/* Export pre-parsed, data-only tables once, then use .h2tb wherever
 * this toolkit previously required a Windows executable. */
static int do_tablesexport(int argc,char **argv){
 int ver;
 if(argc!=5||!parse_version(argv[2],&ver)||!load_pe(argv[3],ver)||
    !hod2_tablepack_export(&pe,argv[4]))return 0;
 printf("H2TB table archive written: %s (1999=%d, no executable code)\n",argv[4],ver==0);
 return 1;
}
static int do_tablesdump(int argc,char **argv){
 int ver;
 if(argc!=5||!parse_version(argv[2],&ver)||!load_pe(argv[3],ver)||
    !hod2_tablepack_dump_csv(&pe,argv[4]))return 0;
 printf("HOD2 data tables exported to CSV in: %s\n",argv[4]);return 1;
}
static int do_tablescheck(int argc,char **argv){
 int ver;unsigned long i;const unsigned char *d;
 if(argc!=4||!parse_version(argv[2],&ver)||!load_pe(argv[3],ver)||
    pe.file.size<48UL||memcmp(pe.file.data,"H2TB",4UL)!=0)return 0;
 d=pe.file.data;
 printf("H2TB v%lu profile=%d segments=%lu bytes=%lu\n",hod2_u32(d+4),ver,
        hod2_u32(d+12),(unsigned long)pe.file.size);
 for(i=0UL;i<hod2_u32(d+12);i++)printf("segment %lu: virtual=0x%08lx size=%lu FNV32=0x%08lx\n",
  i,hod2_u32(d+16UL+i*16UL),hod2_u32(d+20UL+i*16UL),hod2_u32(d+28UL+i*16UL));
 return 1;
}
static int palette_for(const Hod2PE *p,unsigned long bank,unsigned int idx,unsigned long fmt){
 unsigned long palid=0UL,ptr,root;const unsigned char *raw;unsigned int i;unsigned long v;
 if(bank==0x147UL){
  if(!hod2_pe_data(p,hod2_pe_shift(p,0x0057a524UL,1)+(unsigned long)idx*2UL,2,&raw))return 0;
  palid=(unsigned long)hod2_s16(raw);
 }else if(bank==0x177UL)palid=10UL;
 if(palid>=50UL)return 0;
 root=hod2_pe_shift(p,0x0057a010UL,1)+(unsigned long)palid*12UL;
 if(!hod2_pe_data(p,root,12,&raw)||hod2_u16(raw+2)!=16UL||hod2_u32(raw+8)!=16UL)return 0;
 ptr=hod2_u32(raw+4);
 if(!hod2_pe_data(p,ptr,32,&raw))return 0;
 for(i=0;i<16U;i++){
  v=hod2_u16(raw+i*2U);
  if(fmt==1UL)v=(((v&0xffe0UL)<<1)|(v&31UL))&0xffffUL;
  else if(fmt==2UL)v=((v&0x8000UL)?0xf000UL:0UL)|(((v>>11)&15UL)<<8)|(((v>>6)&15UL)<<4)|((v>>1)&15UL);
  palette[i]=v;
 }
 return 1;
}
static int make_path(char *dst,const char *dir,const char *bank){size_t a=strlen(dir),b=strlen(bank);
 if(a+b+2UL>=HOD2_MAX_PATH)return 0;
 memcpy(dst,dir,a);if(a&&dir[a-1]!='/'&&dir[a-1]!='\\'){dst[a++]='/';}memcpy(dst+a,bank,b+1UL);return 1;
}
static int do_pol(int argc,char **argv){unsigned long r,e,model,i=0UL;unsigned int cnt=0;Hod2PolyStat s;
 if(argc<3||!load_data(argv[2]))return 0;
 if(argc>=4&&(!number(argv[3],&model)||model>4095UL))return 0;
 if(!hod2_pol_model(decoded,file_size,0U,&r,&e,&cnt))return 0;
 printf("model,materials,strips,vertices,triangles,nonfinite_uv,alt_strips,untextured\n");
 for(i=0UL;i<cnt;i++){
  if(argc>=4&&i!=model)continue;
  if(!hod2_pol_model(decoded,file_size,(unsigned int)i,&r,&e,(unsigned int*)0)||!hod2_pol_scan(decoded,file_size,r,e,&s,(Hod2VertexCb)0,(void*)0))return 0;
  printf("%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",i,s.materials,s.strips,s.vertices,s.triangles,s.nonfinite_uv,s.alt_strips,s.untextured);
 }
 return 1;
}
static int do_glb(int argc,char **argv){unsigned long model;if(argc!=5||!number(argv[3],&model)||model>4095UL||!load_data(argv[2]))return 0;
 if(!hod2_export_glb(argv[4],decoded,file_size,(unsigned int)model))return 0;
 printf("GLB geometry+UV created: %s (static mesh, original TEX not embedded)\n",argv[4]);return 1;
}
static int do_glbtex(int argc,char **argv){
 int ver;unsigned long bank,idx;size_t polysize;
 if(argc!=9||!parse_version(argv[2],&ver)||!number(argv[5],&idx)||idx>4095UL||!number(argv[7],&bank)||bank>=700UL)return 0;
 if(!load_pe(argv[3],ver)||!load_data(argv[4]))return 0;
 polysize=file_size;memcpy(pol_copy,decoded,polysize);
 if(!load_data(argv[6]))return 0;
 if(!hod2_export_glb_textured(argv[8],pol_copy,polysize,(unsigned int)idx,&pe,(unsigned int)bank,decoded,file_size))return 0;
 printf("GLB original geometry, UV and available TEX textures: %s\n",argv[8]);return 1;
}
static int do_tex(int argc,char **argv){unsigned long bank,idx;Hod2TexDesc d;int ver;const unsigned long *pal=(const unsigned long*)0;
 if(argc!=8||!parse_version(argv[2],&ver)||!number(argv[5],&bank)||!number(argv[6],&idx)||bank>=700UL||idx>=4096UL)return 0;
 if(!load_pe(argv[3],ver)||!load_data(argv[4])||!hod2_tex_desc(&pe,(unsigned int)bank,(unsigned int)idx,&d,(unsigned int*)0))return 0;
 if((d.fmt>>8)==5UL){if(!palette_for(&pe,bank,(unsigned int)idx,d.fmt&7UL))return 0;pal=palette;}
 if(!hod2_tex_png(argv[7],decoded,file_size,&d,pal))return 0;
 printf("PNG %u x %u, fmt=%lu, layout=%lu: %s\n",d.width,d.height,d.fmt&7UL,d.fmt>>8,argv[7]);return 1;
}
static int do_prs(int argc,char **argv){size_t n,outsize;unsigned long want;
 if(argc!=4||!hod2_read_file(argv[2],source,sizeof(source),&n)||n<5)return 0;
 want=hod2_u32(source);if(want>sizeof(decoded)||!hod2_prs_decode(source+4,n-4,decoded,sizeof(decoded),&outsize)||outsize!=(size_t)want)return 0;
 if(!hod2_write_file(argv[3],decoded,outsize))return 0;
 printf("PRS unpacked %lu -> %lu bytes\n",(unsigned long)n,(unsigned long)outsize);return 1;
}
static int do_parts(int argc,char **argv){int ver;unsigned long type;Hod2ExtraExe ex;Hod2ExtraPart part;unsigned int n,i,k;int res;
 if(argc!=5||!parse_version(argv[2],&ver)||!number(argv[4],&type)||type>85UL||!load_pe(argv[3],ver))return 0;
 hod2ex_init(&ex,executable,pe.file.size,ver);
 res=hod2ex_count(&ex,(unsigned int)type,&n);if(res!=1)return 0;
 printf("type,part,slot,mode,drawer,draw_bone,rows\n");
 for(i=0;i<n;i++){
  res=hod2ex_read(&ex,(unsigned int)type,i,&part);
  if(res<0)return 0;
  if(!res){printf("%lu,%u,NULL,-,-,-,-\n",type,i);continue;}
  printf("%lu,%u,%lu,%d,0x%08lx,%ld,%lu\n",type,i,part.slot,part.draw_mode,part.drawer_va,part.draw_bone,part.row_count);
  for(k=0;k<4U;k++)if(part.source_vertex_count[k]>0UL)fprintf(stderr," group=%u bone=%ld src=%lu\n",k,part.group_bone[k],part.source_vertex_count[k]);
 }
 return 1;
}
static int do_clips(int argc,char **argv){int ver;unsigned long type;unsigned int i,id,prov;
 if(argc!=4||!parse_version(argv[2],&ver)||!number(argv[3],&type)||type>=HOD2_CHARACTER_TYPES)return 0;
 printf("type,which,motion_id,provenance\n");
 for(i=0U;i<2U;i++)if(hod2_character_clip((unsigned int)ver,(unsigned int)type,i,&id,&prov))printf("%lu,%u,%u,%u\n",type,i,id,prov);
 return 1;
}
static int do_skel(int argc,char **argv){int ver;unsigned long type;unsigned int n,i;
 if(argc!=5||!parse_version(argv[2],&ver)||!number(argv[4],&type)||type>85UL||!load_pe(argv[3],ver))return 0;
 if(!hod2_skeleton(&pe,(unsigned int)type,nodes,HOD2_MAX_NODES,&n))return 0;
 printf("bone_id,parent,model_id,offset_x_bits,offset_y_bits,offset_z_bits\n");
 for(i=0U;i<n;i++)printf("%u,%u,%u,0x%08lx,0x%08lx,0x%08lx\n",nodes[i].id,nodes[i].parent,nodes[i].mid,nodes[i].pos_bits[0],nodes[i].pos_bits[1],nodes[i].pos_bits[2]);
 return 1;
}
static int do_audit(int argc,char **argv){
 int ver;unsigned long type,refsum=0UL,mat=0UL,faces=0UL,bad=0UL,missing=0UL;unsigned int n,i,index;Hod2Bank bank;Hod2PolyStat s;unsigned long r,e;FILE *report;char path[HOD2_MAX_PATH];
 if(argc!=6||!parse_version(argv[2],&ver)||!load_pe(argv[3],ver))return 0;
 report=fopen(argv[5],"wb");if(!report)return 0;
 memset(seen_models,0,sizeof(seen_models));
 fprintf(report,"type,bones,model_references,nonfinite_uv,failed_models,skeleton_missing\n");
 for(type=0UL;type<86UL;type++){
  unsigned long refs=0UL,uv=0UL,fail=0UL;
  if(!hod2_skeleton(&pe,(unsigned int)type,nodes,HOD2_MAX_NODES,&n)){
   fprintf(report,"%lu,0,0,0,0,1\n",type);missing++;continue;
  }
  for(i=0U;i<n;i++){
   unsigned int mid=nodes[i].mid;
   if(!mid)continue;
   refs++;
   if(mid>=32768U){fail++;continue;}
   if(seen_models[mid]==1U){uv+=uv_model[mid];continue;}
   if(seen_models[mid]==2U){fail++;continue;}
   if(!hod2_find_model(&pe,mid,&bank,&index)||!make_path(path,argv[4],bank.name)||!load_data(path)||!hod2_pol_model(decoded,file_size,index,&r,&e,(unsigned int*)0)||!hod2_pol_scan(decoded,file_size,r,e,&s,(Hod2VertexCb)0,(void*)0)){
    seen_models[mid]=2U;fail++;continue;
   }
   seen_models[mid]=1U;uv_model[mid]=s.nonfinite_uv;uv+=s.nonfinite_uv;mat+=s.materials;faces+=s.triangles;
  }
  refsum+=refs;bad+=fail;
  fprintf(report,"%lu,%u,%lu,%lu,%lu,0\n",type,n,refs,uv,fail);
 }
 if(fclose(report))return 0;
 printf("audit reference-count=%lu parsed-materials=%lu triangles=%lu failures=%lu missing_skeletons=%lu (unique model cache)\n",refsum,mat,faces,bad,missing);
 return bad==0UL && missing==0UL;
}
static int do_peprobe(int argc,char **argv){
 static const unsigned long probes99[]={0x00419980UL,0x00419e50UL,0x00419b40UL,0x004534a0UL,0x0052ed08UL,0x004ed1e0UL,0x004edaecUL};
 static const unsigned long probes00[]={0x00419a50UL,0x00419f20UL,0x00419c10UL,0x00453030UL,0x0052ed08UL,0x004ed1e0UL,0x004edaecUL};
 const unsigned long *values;const unsigned char *p;FILE *f;unsigned int i,j;unsigned long addr;int ver;
 if(argc!=5||!parse_version(argv[2],&ver)||!load_pe(argv[3],ver))return 0;
 f=fopen(argv[4],"wb");if(!f)return 0;
 values=ver?probes00:probes99;
 fprintf(f,"HOD2 C89 static PE probe\nprofile=%d\nsize=%lu\n",ver,(unsigned long)pe.file.size);
 for(i=0U;i<7U;i++){
  addr=values[i];
  if(ver&&i>=4U)addr=hod2_pe_shift(&pe,addr,0);
  fprintf(f,"target=0x%08lx name=%s mapped=",addr,i==3U?"CharacterDrawBonePart":"target");
  if(!hod2_pe_data(&pe,addr,16U,&p)){fputs("no\n",f);continue;}
  fputs("yes bytes=",f);
  for(j=0U;j<16U;j++)fprintf(f,"%02x",(unsigned int)p[j]);
  fputc('\n',f);
 }
 if(fclose(f))return 0;
 printf("PE static probe: %s (raw bytes, not a decompiler)\n",argv[4]);return 1;
}
static int do_motions(int argc,char **argv){
 int ver;unsigned long type,count=0UL;
 if(argc!=7||!parse_version(argv[2],&ver)||!number(argv[5],&type)||type>85UL||!load_pe(argv[3],ver))return 0;
 if(!hod2_mot_list(&pe,argv[4],(unsigned int)type,argv[6],&count))return 0;
 printf("MOT candidate clips: %lu; CSV: %s\n",count,argv[6]);return 1;
}
static int do_motdump(int argc,char **argv){
 int ver;unsigned long type,mot;
 if(argc!=8||!parse_version(argv[2],&ver)||!number(argv[5],&type)||type>85UL||!number(argv[6],&mot)||mot>32767UL||!load_pe(argv[3],ver))return 0;
 if(!hod2_mot_dump(&pe,argv[4],(unsigned int)type,(unsigned int)mot,argv[7]))return 0;
 printf("MOT native block preserved: %s\n",argv[7]);return 1;
}
static int do_glbcheck(int argc,char **argv){unsigned long j,b;if(argc!=3||!hod2_glb_check(argv[2],&j,&b))return 0;
 printf("GLB structure OK json=%lu binary=%lu\n",j,b);return 1;
}
static int do_character_glb(int argc,char **argv){
 int ver;unsigned long type;unsigned int have=0U,missing=0U,tex=0U;
 const char *out,*mot=(const char*)0,*texdir=(const char*)0;
 if(argc<7||argc>9||!parse_version(argv[2],&ver)||
    !number(argv[4],&type)||type>=86UL||!load_pe(argv[3],ver))return 0;
 tex=(strcmp(argv[1],"charactertex")==0);
 if(tex){
  if(argc!=8&&argc!=9)return 0;
  texdir=argv[6];out=argv[7];if(argc==9)mot=argv[8];
 }else{
  if(argc!=7&&argc!=8)return 0;
  out=argv[6];if(argc==8)mot=argv[7];
 }
 if(!hod2_export_character_glb(out,&pe,(unsigned int)type,argv[5],
    mot,&have,&missing,texdir))return 0;
 printf("character: rigid-bone meshes=%u missing/unsupported rigid parts=%u; extra skinning not exported\n",have,missing);
 return 1;
}
/* Export all 86 known character types in one pass, preserving failures in CSV.
 * Input POL and optional TEX directories contain user-extracted bank files.
 * For clips, use the output of `motdump` as type00.h2mc ... type85.h2mc.
 * No directory creation, no dynamic allocation, no stdin.
 */
static int do_batch_characters(int argc,char **argv){
 int ver,with_tex,with_mot,status;
 unsigned int t,have,missing,finished=0U,failed=0U,clipped=0U;
 unsigned int extra=0U;Hod2ExtraExe ex;
 char out[HOD2_MAX_PATH],mot[HOD2_MAX_PATH];
 const char *outdir,*texdir,*motdir;size_t len;
 FILE *csv,*check;int xstat;
 if(argc!=8&&argc!=9)return 0;
 if(!parse_version(argv[2],&ver)||!load_pe(argv[3],ver))return 0;
 outdir=argv[6];texdir=strcmp(argv[5],"-")?argv[5]:(const char*)0;
 motdir=argc==9?argv[8]:(const char*)0;
 len=strlen(outdir);
 if(len+16UL>=HOD2_MAX_PATH)return 0;
 if(motdir&&strlen(motdir)+16UL>=HOD2_MAX_PATH)return 0;
 csv=fopen(argv[7],"wb");if(!csv)return 0;
 if(fputs("profile,type,rigid_nodes,unresolved_rigid,extra_descriptors,clip_present,export_status,file\n",csv)==EOF){fclose(csv);return 0;}
 hod2ex_init(&ex,pe.file.data,pe.file.size,ver);
 for(t=0U;t<86U;t++){
  sprintf(out,"%s%stype%02u.glb",outdir,(len&&outdir[len-1]=='/')?"":"/",t);
  /* Caller creates output directory; this command never writes outside it. */
  with_mot=0;mot[0]='\0';
  if(motdir){
   size_t mlen=strlen(motdir);
   sprintf(mot,"%s%stype%02u.h2mc",motdir,(mlen&&motdir[mlen-1]=='/')?"":"/",t);
   check=fopen(mot,"rb");
   if(check){fclose(check);with_mot=1;}
  }
  extra=0U;xstat=hod2ex_count(&ex,t,&extra);
  with_tex=texdir!=0;
  have=missing=0U;
  status=hod2_export_character_glb(out,&pe,t,argv[4],with_mot?mot:(const char*)0,
     &have,&missing,with_tex?texdir:(const char*)0);
  if(status){finished++;if(with_mot)clipped++;}else failed++;
  if(fprintf(csv,"%d,%u,%u,%u,%d,%u,%s,%s\n",ver,t,have,missing,
      xstat==1?(int)extra:-1,with_mot?1U:0U,status?"ok":"fail",status?out:"")<0){fclose(csv);return 0;}
 }
 if(fclose(csv)!=0)return 0;
 printf("batch: profile=%d exported=%u failed=%u clips_found=%u; skeleton+rigid only; extra skinning not implemented\n",ver,finished,failed,clipped);
 return failed==0U;
}
static int do_skin_audit(int argc,char **argv){
 int ver;unsigned long matched=0UL,total=0UL,parts=0UL;
 if(argc!=6||!parse_version(argv[2],&ver)||!load_pe(argv[3],ver))return 0;
 if(!hod2_skin_audit(&pe,argv[4],argv[5],&matched,&total,&parts))return 0;
 printf("skin audit: profile=%d, parts=%lu matched_offsets=%lu total_offsets=%lu CSV=%s\n",ver,parts,matched,total,argv[5]);
 return 1;
}
static int do_skin_map(int argc,char **argv){
 int ver;unsigned long type,part,refs=0UL,overrides=0UL;
 if(argc!=8||!parse_version(argv[2],&ver)||!load_pe(argv[3],ver)||
   !number(argv[5],&type)||type>=86UL||!number(argv[6],&part)||part>=31UL)return 0;
 if(!hod2_skin_map(&pe,argv[4],(unsigned int)type,(unsigned int)part,argv[7],&refs,&overrides))return 0;
 printf("skinmap type=%lu part=%lu vertex_references=%lu source_overrides=%lu report=%s\n",
        type,part,refs,overrides,argv[7]);return 1;
}
static int do_tail23(int argc,char **argv){
 int ver;unsigned long found=0UL;
 if(argc!=5||!parse_version(argv[2],&ver)||!load_pe(argv[3],ver)||
    !hod2_tail23_report(&pe,argv[4],&found))return 0;
 printf("tail23: segments with confirmed model references=%lu CSV=%s\n",found,argv[4]);return 1;
}
static int do_skin_character(int argc,char **argv){
 int ver;unsigned long type;const char *tex;
 if(argc!=8&&argc!=9)return 0;
 if(!parse_version(argv[2],&ver)||!number(argv[4],&type)||type>=86UL||!load_pe(argv[3],ver))return 0;
 tex=argc==9&&strcmp(argv[8],"-")?argv[8]:(const char*)0;
 if(!hod2_export_skinned_character_glb(argv[7],&pe,(unsigned int)type,argv[5],argv[6],tex))return 0;
 printf("skincharacter type=%lu exported all supported rigid+deformable parts with MOT: %s\n",type,argv[7]);return 1;
}
static int do_skin_glb(int argc,char **argv){
 int ver;unsigned long type,part;const char *tex;
 if(argc!=9&&argc!=10)return 0;
 if(!parse_version(argv[2],&ver)||!number(argv[4],&type)||type>=86UL||
    !number(argv[5],&part)||part>=31UL||!load_pe(argv[3],ver))return 0;
 tex=argc==10&&strcmp(argv[9],"-")?argv[9]:(const char*)0;
 if(!hod2_export_skinned_part_glb(argv[8],&pe,(unsigned int)type,(unsigned int)part,argv[6],argv[7],tex))return 0;
 printf("skinpart: type=%lu part=%lu GLB with EXE vertex groups and MOT skin: %s\n",type,part,argv[8]);return 1;
}
static void usage(void){
 puts("HOD2 C89 native toolkit | no heap / no floats / no stdin");
 puts("  hod2c89 tablesexport <1999|2000> <original.exe> <tables.h2tb>   [one-time]");
 puts("  hod2c89 tablescheck <1999|2000> <tables.h2tb>");
 puts("  hod2c89 tablesdump <1999|2000> <tables.h2tb> <existing_output_folder>");
 puts("  Replace <exe> with <tables.h2tb> for data-oriented commands below:");
 puts("  hod2c89 prs <input.prs.bin> <output.bin>");
 puts("  hod2c89 corpus <resource_root> <audit.csv>    [real POL/MOT/TEX without EXE]");
 puts("  hod2c89 corpusdiff <1999_root> <2000_root> <report.csv>");
 puts("  hod2c89 pol <pol.bin> [model_index]");
 puts("  hod2c89 glb <pol.bin> <model_index> <mesh.glb>     [geometry, UV only]");
 puts("  hod2c89 glbcheck <mesh.glb>");
 puts("  hod2c89 glbtex <1999|2000> <exe> <pol.bin> <model_index> <tex.bin> <bank_index> <model.glb>");
 puts("  hod2c89 tex <1999|2000> <exe> <tex.bin> <bank_index> <tex_index> <out.png>");
 puts("  hod2c89 skeleton <1999|2000> <exe> <type_0_to_85>");
 puts("  hod2c89 parts <1999|2000> <exe> <type_0_to_85>");
 puts("  hod2c89 peprobe <1999|2000> <exe> <report.txt>");
 puts("  hod2c89 clips <1999|2000> <type_0_to_85>");
 puts("  hod2c89 audit <1999|2000> <exe> <extracted_pol_folder> <audit.csv>");
 puts("  hod2c89 skinaudit <1999|2000> <exe> <extracted_pol_folder> <skin.csv>");
 puts("  hod2c89 skinmap <1999|2000> <exe> <pol_folder> <type> <part> <out.csv>");
 puts("  hod2c89 skincharacter <1999|2000> <exe> <type> <pol_dir> <motion.h2mc> <out.glb> [tex_dir|->]");
 puts("  hod2c89 skinpart <1999|2000> <exe> <type> <part> <pol_dir> <motion.h2mc> <out.glb> [tex_dir|->]");
 puts("  hod2c89 tail23 <1999|2000> <exe> <tail_segments.csv>");
 puts("  hod2c89 motions <1999|2000> <exe> <extracted_mot_folder> <type> <clips.csv>");
 puts("  hod2c89 batch <1999|2000> <exe> <pol_folder> <tex_folder|-> <out_dir> <report.csv> [typeNN.h2mc_folder]");
 puts("  hod2c89 charactertex <1999|2000> <exe> <type> <pol_folder> <tex_folder> <out.glb> [motion.h2mc]");
 puts("  hod2c89 character <1999|2000> <exe> <type> <extracted_pol_folder> <out.glb> [motion.h2mc]");
 puts("  hod2c89 motdump <1999|2000> <exe> <extracted_mot_folder> <type> <motion_id> <clip.h2mc>");
}
int main(int argc,char **argv){int ok=0;
 if(argc<2){usage();return 0;}
 if(!strcmp(argv[1],"tablesexport"))ok=do_tablesexport(argc,argv);
 else if(!strcmp(argv[1],"tablescheck"))ok=do_tablescheck(argc,argv);
 else if(!strcmp(argv[1],"tablesdump"))ok=do_tablesdump(argc,argv);
 else if(!strcmp(argv[1],"prs"))ok=do_prs(argc,argv);
 else if(!strcmp(argv[1],"pol"))ok=do_pol(argc,argv);
 else if(!strcmp(argv[1],"corpus"))ok=argc==4 && hod2_corpus_audit(argv[2],argv[3]);
 else if(!strcmp(argv[1],"corpusdiff"))ok=argc==5 && hod2_corpus_diff(argv[2],argv[3],argv[4]);
 else if(!strcmp(argv[1],"glb"))ok=do_glb(argc,argv);
 else if(!strcmp(argv[1],"glbcheck"))ok=do_glbcheck(argc,argv);
 else if(!strcmp(argv[1],"glbtex"))ok=do_glbtex(argc,argv);
 else if(!strcmp(argv[1],"tex"))ok=do_tex(argc,argv);
 else if(!strcmp(argv[1],"skeleton"))ok=do_skel(argc,argv);
 else if(!strcmp(argv[1],"parts"))ok=do_parts(argc,argv);
 else if(!strcmp(argv[1],"peprobe"))ok=do_peprobe(argc,argv);
 else if(!strcmp(argv[1],"clips"))ok=do_clips(argc,argv);
 else if(!strcmp(argv[1],"audit"))ok=do_audit(argc,argv);
 else if(!strcmp(argv[1],"skinaudit"))ok=do_skin_audit(argc,argv);
 else if(!strcmp(argv[1],"skinmap"))ok=do_skin_map(argc,argv);
 else if(!strcmp(argv[1],"skincharacter"))ok=do_skin_character(argc,argv);
 else if(!strcmp(argv[1],"skinpart"))ok=do_skin_glb(argc,argv);
 else if(!strcmp(argv[1],"tail23"))ok=do_tail23(argc,argv);
 else if(!strcmp(argv[1],"motions"))ok=do_motions(argc,argv);
 else if(!strcmp(argv[1],"batch"))ok=do_batch_characters(argc,argv);
 else if(!strcmp(argv[1],"character")||!strcmp(argv[1],"charactertex"))ok=do_character_glb(argc,argv);
 else if(!strcmp(argv[1],"motdump"))ok=do_motdump(argc,argv);
 else if(!strcmp(argv[1],"help")||!strcmp(argv[1],"--help")){usage();return 0;}
 else{usage();return 2;}
 if(!ok){fprintf(stderr,"HOD2 ERROR: %s (%s)\n",errmsg,argv[1]);return 1;}
 return 0;
}
