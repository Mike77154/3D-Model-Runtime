#include "bc.h"
#include "bc_rig_q16.h"
#include "bc_ik2.h"
#include "bc_ik_chain.h"
#include <stdio.h>
#include <string.h>
/* Huge fixed-capacity backing store; never instantiate on the stack. */
static BC_File data;
#define BC_MULTI_MAX 256U
static BC_RigPair bc_multi_pairs[BC_MULTI_MAX];
int main(int argc,char **argv)
{
  FILE *out; unsigned int i,meshes=0U,datablocks=0U,valid=0U;
  int rc, sample_frame=0;
  const BC_Struct *mesh;
  if(argc<3 || (strcmp(argv[1],"--info")!=0 && strcmp(argv[1],"--obj")!=0 &&
                strcmp(argv[1],"--schema")!=0 && strcmp(argv[1],"--blocks")!=0 && strcmp(argv[1],"--meshes")!=0 && strcmp(argv[1],"--weights")!=0 && strcmp(argv[1],"--bones")!=0 && strcmp(argv[1],"--keys")!=0 && strcmp(argv[1],"--pose")!=0 && strcmp(argv[1],"--ipo")!=0 && strcmp(argv[1],"--sample")!=0 && strcmp(argv[1],"--rest-matrices")!=0 && strcmp(argv[1],"--skin-tsv")!=0 && strcmp(argv[1],"--palette-tsv")!=0 && strcmp(argv[1],"--animate-tsv")!=0 && strcmp(argv[1],"--play-blend")!=0 && strcmp(argv[1],"--play-obj")!=0 && strcmp(argv[1],"--play-sequence")!=0 && strcmp(argv[1],"--objects")!=0 && strcmp(argv[1],"--bindings")!=0 && strcmp(argv[1],"--nla")!=0 && strcmp(argv[1],"--rig-timeline")!=0 && strcmp(argv[1],"--play-rigs")!=0 && strcmp(argv[1],"--rig-pairs")!=0 && strcmp(argv[1],"--ik2-palette")!=0 && strcmp(argv[1],"--ik-chain")!=0 && strcmp(argv[1],"--ik-audit")!=0)) {
    fputs("usage: blendc89 --info file.blend | --obj file.blend out.obj | --sample file.blend sampled.tsv frame\n",stderr);
    fputs("       --animate-tsv rest.tsv sample.tsv vertices.tsv output.tsv [action_id]\n",stderr);return 2;
  }
  if(strcmp(argv[1],"--ik-chain")==0){
    if(argc!=5 ||bc_ik_chain_cli(argv[2],argv[3],argv[4])!=BC_RIG_OK){
      fputs("usage: --ik-chain chain.tsv deform_palette.tsv solved_joints.tsv\n",stderr);
      return 3;
    }
    printf("IK multi-link solved: %s\n",argv[4]);return 0;
  }
  if(strcmp(argv[1],"--ik2-palette")==0){
    if(argc!=5 || bc_ik2_cli(argv[2],argv[3],argv[4])!=BC_RIG_OK){
      fputs("usage: --ik2-palette goals.tsv deform_palette.tsv joints.tsv\n",stderr);
      return 3;
    }
    printf("IK two-bone deformation palette: %s\n",argv[3]);return 0;
  }
  if(strcmp(argv[1],"--animate-tsv")==0){
    unsigned int aid=0U; const char *p;
    if(argc!=6 && argc!=7)return 2;
    if(argc==7){
      p=argv[6];if(!*p)return 2;
      while(*p){
        if(*p<'0'||*p>'9'||aid>(4294967295U-(unsigned int)(*p-'0'))/10U)return 2;
        aid=aid*10U+(unsigned int)(*p-'0');p++;
      }
    }
    if(bc_playback_tsv(argv[2],argv[3],argv[4],argv[5],aid)!=BC_OK){
      fprintf(stderr,"animate-tsv: unsupported/invalid rest, channel samples or vertex weights\n");
      return 3;
    }
    printf("Animated vertices written: %s\n",argv[5]);return 0;
  }
  if(strcmp(argv[1],"--palette-tsv")==0){
    if(argc!=5 || bc_palette_tsv(argv[2],argv[3],argv[4])!=BC_OK){
      fprintf(stderr,"palette-tsv: failed to compose matrices\n");return 3;
    }
    printf("Deform palette written: %s\n",argv[4]);return 0;
  }
  if(strcmp(argv[1],"--skin-tsv")==0){
    if(argc!=5 || bc_skin_tsv(argv[2],argv[3],argv[4])!=BC_OK){
      fprintf(stderr,"skin-tsv: invalid palette, vertex influences, or output\n");return 3;
    }
    printf("Skinned vertices written: %s\n",argv[4]);return 0;
  }
  rc=bc_open(&data,argv[2]);
  if(rc<0){fprintf(stderr,"ERROR: %s\n",data.error);bc_close(&data);return 1;}
  rc=bc_scan(&data);
  if(rc<0){fprintf(stderr,"ERROR: %s\n",data.error);bc_close(&data);return 1;}
  rc=bc_dna_parse(&data);
  if(rc<0){fprintf(stderr,"ERROR: invalid/unsupported SDNA catalog\n");bc_close(&data);return 1;}
  for(i=0;i<data.nstructs;i++)if(data.structs[i].valid)++valid;
  mesh=bc_get_struct(&data,"Mesh");
  for(i=0;i<data.nblocks;i++) {
    if(data.blocks[i].code[0]=='M' && data.blocks[i].code[1]=='E')++meshes;
    if(data.blocks[i].code[0]=='D' && data.blocks[i].code[1]=='A' &&
       data.blocks[i].code[2]=='T' && data.blocks[i].code[3]=='A')++datablocks;
  }
  if(strcmp(argv[1],"--info")==0){
    printf("BLEND version: %d\nHeader generation: %d\nPointer width: %d\nEndianness: %s\n",
      data.file_version,data.format_version,data.ptr_size,data.endian_le?"little":"big");
    printf("Blocks: %u (ME: %u DATA: %u)\nSDNA names: %u types: %u structs: %u fields: %u\n",
      data.nblocks,meshes,datablocks,data.nnames,data.ntypes,data.nstructs,data.nfields);
    printf("SDNA layouts validated: %u / %u\n",valid,data.nstructs);
    printf("Mesh schema: %s\n",mesh?(mesh->valid?"recognized":"layout mismatch") : "absent");
  } else if(strcmp(argv[1],"--schema")==0) {
    const BC_Struct *s;
    if(argc<4){fprintf(stderr,"missing type name\n");bc_close(&data);return 2;}
    s=bc_get_struct(&data,argv[3]);
    if(!s){fprintf(stderr,"SDNA type not found\n");bc_close(&data);return 3;}
    printf("struct %s (saved size %u, layout %s):\n",argv[3],
       (unsigned int)data.type_len[s->type],s->valid?"matching":"NOT MATCHING");
    for(i=0;i<s->nfield;i++) {
      const BC_Field *f=&data.fields[s->first+i];
      printf("  +%u  %s %s\n",f->offset,bc_type(&data,f->type),bc_name(&data,f->name));
    }
  } else if(strcmp(argv[1],"--blocks")==0) {
    for(i=0;i<data.nblocks;i++) {
      const BC_Block *p=&data.blocks[i];
      printf("%5u  %.4s  %10u  count=%u sdna=%u address=%08x:%08x\n",
             i,p->code,p->size,p->count,p->sdna,p->old.hi,p->old.lo);
    }
  } else if(strcmp(argv[1],"--meshes")==0) {
    rc=bc_mesh_report(&data,stdout);
    if(rc!=BC_OK){fprintf(stderr,"mesh scan failed or schema unsupported\n");bc_close(&data);return 3;}
  } else if(strcmp(argv[1],"--play-sequence")==0){
    unsigned int aid=0U,slot=0U,ordinal=0U;int filter_slot=0,automode=0;int start,end,current;
    char path[640];const char *p;
    if((argc!=6 && argc!=7)|| bc_parse_q16(argv[3],&start)!=BC_OK ||
       bc_parse_q16(argv[4],&end)!=BC_OK ||start>end ||
       end-start>240*65536 ||strlen(argv[5])>550U){
      fprintf(stderr,"usage: --play-sequence file.blend first_frame last_frame prefix [action_id] (up to 241 frames)\n");
      bc_close(&data);return 2;
    }
    if(argc==7){
      p=argv[6];if(!*p){bc_close(&data);return 2;}
      if(strcmp(p,"auto")==0){automode=1;}else{
      while(*p){
        if(*p<'0'||*p>'9'||aid>(4294967295U-(unsigned int)(*p-'0'))/10U){bc_close(&data);return 2;}
        aid=aid*10U+(unsigned int)(*p-'0');p++;
      }}
    }
    for(current=start;current<=end;){
      int playback_frame=current;
      if(automode && bc_find_rig_active_action(&data,&playback_frame,&aid,&slot,&filter_slot)!=BC_OK){
        fprintf(stderr,"auto: no unique direct rig Action, or unsupported NLA at frame %u\n",ordinal);
        bc_close(&data);return 3;
      }
      if(sprintf(path,"%s_frame%04u.obj",argv[5],ordinal)>=640){bc_close(&data);return 3;}
      rc=bc_play_obj_frame_bound(&data,playback_frame,path,aid,slot,filter_slot);
      if(rc!=BC_OK){
        fprintf(stderr,"--play-sequence: failed at frame index %u; earlier OBJ files remain\n",ordinal);
        bc_close(&data);return 3;
      }
      ordinal++;
      if(end-current<65536)break;
      current+=65536;
    }
    printf("Animated OBJ sequence exported: %u frames\n",ordinal);
  } else if(strcmp(argv[1],"--play-obj")==0){
    unsigned int aid=0U,slot=0U;int filter_slot=0,automode=0; const char *p;
    if((argc!=5 && argc!=6) ||bc_parse_q16(argv[3],&sample_frame)!=BC_OK){
      fprintf(stderr,"usage: --play-obj file.blend frame output.obj [action_id]\n");
      bc_close(&data);return 2;
    }
    if(argc==6){
      p=argv[5];if(!*p){bc_close(&data);return 2;}
      if(strcmp(p,"auto")==0){automode=1;}else{
      while(*p){
        if(*p<'0'||*p>'9'||aid>(4294967295U-(unsigned int)(*p-'0'))/10U){bc_close(&data);return 2;}
        aid=aid*10U+(unsigned int)(*p-'0');p++;
      }}
    }
    if(automode && bc_find_rig_active_action(&data,&sample_frame,&aid,&slot,&filter_slot)!=BC_OK){
      fprintf(stderr,"auto: no unique direct rig Action/slot, or unsupported NLA blend\n");
      bc_close(&data);return 3;
    }
    rc=bc_play_obj_frame_bound(&data,sample_frame,argv[4],aid,slot,filter_slot);
    if(rc!=BC_OK){fprintf(stderr,"--play-obj: unsupported rig, weights, action or mesh\n");bc_close(&data);return 3;}
    printf("Animated OBJ written: %s\n",argv[4]);
  } else if(strcmp(argv[1],"--play-rigs")==0){
    unsigned int count,k;
    char file[640];
    if(argc!=5 ||bc_parse_q16(argv[3],&sample_frame)!=BC_OK ||
       strlen(argv[4])>550U){
      fprintf(stderr,"usage: --play-rigs file.blend frame prefix\n");
      bc_close(&data);return 2;
    }
    if(bc_object_rig_pairs(&data,bc_multi_pairs,BC_MULTI_MAX,&count)!=BC_OK){
      fprintf(stderr,"multi-rig: unsupported mesh/armature modifier graph\n");
      bc_close(&data);return 3;
    }
    for(k=0U;k<count;k++){
      if(sprintf(file,"%s_mesh%04u.obj",argv[4],bc_multi_pairs[k].mesh_index)>=640){
        bc_close(&data);return 3;
      }
      rc=bc_play_rig_pair_frame(&data,sample_frame,file,&bc_multi_pairs[k]);
      if(rc!=BC_OK){
        unsigned int z;
        /* Roll back this command's output files on any rig failure. */
        for(z=0U;z<k;z++){
          sprintf(file,"%s_mesh%04u.obj",argv[4],bc_multi_pairs[z].mesh_index);
          remove(file);
        }
        fprintf(stderr,"multi-rig: unsupported playback for mesh %u\n",
                bc_multi_pairs[k].mesh_index);bc_close(&data);return 3;
      }
    }
    printf("Multi-rig OBJ exported: %u meshes\n",count);
  } else if(strcmp(argv[1],"--rig-timeline")==0){
    if(argc!=5 ||bc_parse_q16(argv[3],&sample_frame)!=BC_OK){
      fprintf(stderr,"usage: --rig-timeline file.blend frame out.tsv\n");
      bc_close(&data);return 2;
    }
    out=fopen(argv[4],"wb");
    if(!out){bc_close(&data);return 1;}
    rc=bc_export_rig_timeline(&data,sample_frame,out);
    if(fclose(out)!=0)rc=BC_FAILURE;
    if(rc!=BC_OK){remove(argv[4]);bc_close(&data);return 3;}
    printf("Rig timeline written: %s\n",argv[4]);
  } else if(strcmp(argv[1],"--play-blend")==0){
    unsigned int aid=0U; const char *p;
    if((argc!=6 && argc!=7) || bc_parse_q16(argv[3],&sample_frame)!=BC_OK){
      fprintf(stderr,"usage: --play-blend file.blend frame vertices.tsv output.tsv [action_id]\n");
      bc_close(&data);return 2;
    }
    if(argc==7){
      p=argv[6];if(!*p){bc_close(&data);return 2;}
      while(*p){
        if(*p<'0'||*p>'9'||aid>(4294967295U-(unsigned int)(*p-'0'))/10U){bc_close(&data);return 2;}
        aid=aid*10U+(unsigned int)(*p-'0');p++;
      }
    }
    rc=bc_play_blend_frame(&data,sample_frame,argv[4],argv[5],aid);
    if(rc!=BC_OK){fprintf(stderr,"--play-blend: unsupported rig, pose, frame or inputs\n");bc_close(&data);return 3;}
    printf("Frame exported to: %s\n",argv[5]);
  } else {
    if(argc<4){fprintf(stderr,"missing output filename\n");bc_close(&data);return 2;}
    if(strcmp(argv[1],"--sample")==0 && (argc<5 || bc_parse_q16(argv[4],&sample_frame)!=BC_OK)){
      fprintf(stderr,"--sample requires signed decimal frame (max 4 fractional digits, magnitude <32768)\n");
      bc_close(&data);return 2;
    }
    out=fopen(argv[3],"wb");
    if(!out){fprintf(stderr,"cannot create output\n");bc_close(&data);return 1;}
    if(strcmp(argv[1],"--obj")==0){
      fprintf(out,"# BLEND C89 fixed16 mesh preview\n");
      rc=bc_extract_obj(&data,out);
    } else if(strcmp(argv[1],"--objects")==0)rc=bc_export_object_links(&data,out);
    else if(strcmp(argv[1],"--ik-audit")==0)rc=bc_export_ik_audit(&data,out);
    else if(strcmp(argv[1],"--rig-pairs")==0)rc=bc_export_rig_pairs(&data,out);
    else if(strcmp(argv[1],"--bindings")==0)rc=bc_export_animation_bindings(&data,out,0);
    else if(strcmp(argv[1],"--nla")==0)rc=bc_export_animation_bindings(&data,out,1);
    else if(strcmp(argv[1],"--weights")==0) rc=bc_export_weights(&data,out);
    else if(strcmp(argv[1],"--bones")==0)rc=bc_export_bones(&data,out);
    else if(strcmp(argv[1],"--keys")==0)rc=bc_export_keys(&data,out);
    else if(strcmp(argv[1],"--ipo")==0)rc=bc_export_ipo(&data,out);
    else if(strcmp(argv[1],"--sample")==0)rc=bc_export_sample(&data,out,sample_frame);
    else if(strcmp(argv[1],"--rest-matrices")==0)rc=bc_export_rest_matrices(&data,out);
    else rc=bc_export_pose(&data,out);
    fclose(out);
    if(rc!=BC_OK){remove(argv[3]);fprintf(stderr,"requested serialized data unavailable, corrupt, or unsupported\n");bc_close(&data);return 3;}
    printf("File extracted: %s\n",argv[3]);
  }
  bc_close(&data);return 0;
}
