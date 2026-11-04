/* Export human-readable tables from PE or a standalone H2TB data pack.
 * Strict ANSI C89, no heap, no floating point and no stdin. */
#include "hod2_native.h"
#include "../inherited/c89/hod2_extra_parts.h"
#include <string.h>
static Hod2Node csv_nodes[HOD2_MAX_NODES];
static int path(char out[HOD2_MAX_PATH],const char *dir,const char *file){
 size_t n=strlen(dir),k=strlen(file);
 if(n+k+2UL>=HOD2_MAX_PATH)return 0;
 memcpy(out,dir,n);
 if(n&&dir[n-1]!='/'&&dir[n-1]!='\\')out[n++]='/';
 memcpy(out+n,file,k+1UL);return 1;
}
static int dump_skeletons(const Hod2PE *pe,const char *dir){
 char dest[HOD2_MAX_PATH];FILE *f;unsigned int t,n,i;
 if(!path(dest,dir,"skeletons.csv"))return 0;
 f=fopen(dest,"wb");if(!f)return 0;
 fputs("type,node,parent,model,offset_x_ieee_bits,offset_y_ieee_bits,offset_z_ieee_bits,source_va\n",f);
 for(t=0U;t<86U;t++){
  if(!hod2_skeleton(pe,t,csv_nodes,HOD2_MAX_NODES,&n)){fclose(f);return 0;}
  for(i=0U;i<n;i++){
   Hod2Node *p=&csv_nodes[i];
   if(fprintf(f,"%u,%u,%u,%u,0x%08lx,0x%08lx,0x%08lx,0x%08lx\n",t,p->id,p->parent,p->mid,
    p->pos_bits[0],p->pos_bits[1],p->pos_bits[2],p->va)<0){fclose(f);return 0;}
  }
 }
 return fclose(f)==0;
}
static int dump_parts(const Hod2PE *pe,const char *dir){
 char dest[HOD2_MAX_PATH];FILE *f;unsigned int t,n,i,k;
 Hod2ExtraExe ex;Hod2ExtraPart part;int ret;
 if(!path(dest,dir,"extra_parts.csv"))return 0;
 f=fopen(dest,"wb");if(!f)return 0;
 fputs("type,part,found,slot,draw_mode,draw_bone,drawer_va,mesh_info_va,rows_va,row_count,row_stride,row_ptr_count,group,group_bone,source_count,source_vertices_va,assignments_va\n",f);
 hod2ex_init(&ex,pe->file.data,pe->file.size,pe->profile);
 for(t=0U;t<86U;t++){
  if(hod2ex_count(&ex,t,&n)!=1){fclose(f);return 0;}
  for(i=0U;i<n;i++){
   ret=hod2ex_read(&ex,t,i,&part);
   if(ret<0){fclose(f);return 0;}
   if(!ret){if(fprintf(f,"%u,%u,0,,,,,,,,,,,,,,\n",t,i)<0){fclose(f);return 0;}continue;}
   for(k=0U;k<4U;k++){
    if(fprintf(f,"%u,%u,1,%lu,%d,%ld,0x%08lx,0x%08lx,0x%08lx,%lu,%lu,%lu,%u,%ld,%lu,0x%08lx,0x%08lx\n",
      t,i,part.slot,part.draw_mode,part.draw_bone,part.drawer_va,part.mesh_info_va,
      part.rows_va,part.row_count,part.row_stride,part.row_ptr_count,k,part.group_bone[k],
      part.source_vertex_count[k],part.source_vertices_va[k],part.assignment_va[k])<0){fclose(f);return 0;}
   }
  }
 }
 return fclose(f)==0;
}
static int dump_banks(const Hod2PE *pe,const char *dir,int motion){
 char dest[HOD2_MAX_PATH];FILE *f;unsigned int b,i;long id;Hod2Bank bank;
 if(!path(dest,dir,motion?"motion_banks.csv":"model_banks.csv"))return 0;
 f=fopen(dest,"wb");if(!f)return 0;
 fputs("bank_index,bank_file,entry_index,resource_id,id_table_va\n",f);
 for(b=0U;b<HOD2_MAX_BANKS;b++){
  if(!hod2_bank(pe,b,motion,&bank))continue;
  /* Some out-of-range bank slots accidentally point to machine code,
   * not resource-ID arrays (e.g. pol bank 460). Never treat code as tables. */
  if(bank.id_va<(pe->profile?0x004c3000UL:0x004c4000UL))continue;
  for(i=0U;i<bank.count;i++){
   if(!hod2_bank_id(pe,&bank,i,&id)){fclose(f);return 0;}
   if(fprintf(f,"%u,%s,%u,%ld,0x%08lx\n",b,bank.name,i,id,bank.id_va)<0){fclose(f);return 0;}
  }
 }
 return fclose(f)==0;
}
static int dump_textures(const Hod2PE *pe,const char *dir){
 char dest[HOD2_MAX_PATH];FILE *f;unsigned int b,i,n;Hod2TexDesc d;
 if(!path(dest,dir,"texture_descriptors.csv"))return 0;
 f=fopen(dest,"wb");if(!f)return 0;
 fputs("bank_index,texture_index,width,height,fmt,offset,slot\n",f);
 for(b=0U;b<HOD2_MAX_BANKS;b++){
  if(!hod2_tex_desc(pe,b,0U,&d,&n))continue;
  for(i=0U;i<n;i++){
   if(!hod2_tex_desc(pe,b,i,&d,(unsigned int*)0)){fclose(f);return 0;}
   if(fprintf(f,"%u,%u,%u,%u,%lu,%lu,%lu\n",b,i,d.width,d.height,d.fmt,d.offset,d.slot)<0){fclose(f);return 0;}
  }
 }
 return fclose(f)==0;
}
int hod2_tablepack_dump_csv(const Hod2PE *pe,const char *dir){
 return pe&&dir&&dump_skeletons(pe,dir)&&dump_parts(pe,dir)&&
   dump_banks(pe,dir,0)&&dump_banks(pe,dir,1)&&dump_textures(pe,dir);
}
