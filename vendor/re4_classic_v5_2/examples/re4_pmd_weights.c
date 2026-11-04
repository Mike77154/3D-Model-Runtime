/* RE4 2007 PMD geometry/weight validation. Standalone C89, no heap.
   Diagnostics only; geometry acceptance does not certify canonical assembly. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CAP (16UL*1024UL*1024UL)
#define POOL (20UL*1024UL*1024UL)
#define ONE 65536L
static re4_u8 raw[CAP],pool[POOL];
static re4_pmd_model model;
static re4_u32 error_count,near_weight_count,zero_sum_count,bad_slot_count;
static re4_u32 vertex_count,rigid_count,skinned_count,negative_weight_count;
static re4_u32 zero_triangle_count,bad_triangle_count,bad_bone_count,read_errors;
static re4_u32 min_weight=4294967295UL,max_weight;
static re4_u32 max_deviation;
static re4_u32 extreme_positions;
static re4_u32 bad_slot_example_node=4294967295UL,bad_slot_example_vertex;
static re4_u32 bad_slot_example_index;
static re4_u32 raw_invalid_bone_id;
static re4_u32 err_first_node=4294967295UL,err_first_vertex;
static unsigned long absolute_q16(re4_i32 v){return v<0?(unsigned long)(-(v+1))+1UL:(unsigned long)v;}
static void check_vertex(re4_span src,const re4_pmd_node *node,const re4_pmd_mesh *mesh,re4_u32 ni,re4_u32 vi)
{
    re4_pmd_vertex_q16 v;re4_u32 k,slot;re4_i32 w0,w1;re4_i32 idx;
    re4_err e=re4_pmd_mesh_vertex(src,mesh,vi,&v);
    if(e!=RE4_OK){++read_errors;if(err_first_node==4294967295UL){err_first_node=ni;err_first_vertex=vi;}return;}
    ++vertex_count;
    w0=v.weights[0];w1=v.weights[1];
    if(w0<0||w1<0){++negative_weight_count;return;}
    {
      re4_u32 sum=(re4_u32)w0+(re4_u32)w1;
      re4_u32 delta=sum>ONE?sum-ONE:ONE-sum;
      if(sum<min_weight)min_weight=sum;
      if(sum>max_weight)max_weight=sum;
      if(delta>max_deviation)max_deviation=delta;
      if(sum==0UL)++zero_sum_count;
      else if(delta>2048UL)++near_weight_count;
    }
    for(k=0;k<2U;++k){
        idx=v.bone_indices[k];
        if((k==0U?w0:w1)==0)continue;
        if(idx<0){++bad_slot_count;continue;}
        /* PMD attributes are expressed as float slot*3, converted to Q16. */
        slot=(re4_u32)(((unsigned long)idx+98304UL)/196608UL);
        if(node->node_bone_count==0U){
            if(idx!=0){++bad_slot_count;}
        }else if(slot>=node->node_bone_count ||
            absolute_q16(idx-(re4_i32)(slot*196608UL))>16384UL){
            ++bad_slot_count;
            if(bad_slot_example_node==4294967295UL){bad_slot_example_node=ni;bad_slot_example_vertex=vi;bad_slot_example_index=(re4_u32)idx;}
        }
    }
    if(node->node_bone_count==0U)++rigid_count;else ++skinned_count;
    for(k=0;k<3U;++k)if(absolute_q16(v.position[k])>100000000UL){++extreme_positions;break;}
}
int main(int argc,char **argv)
{
    FILE *f;long sz;re4_span src;re4_arena a;re4_u32 i,j,k;re4_err err;
    if(argc!=2){fputs("usage: re4_pmd_weights model.pmd\n",stderr);return 2;}
    f=fopen(argv[1],"rb");if(!f)return 3;
    if(fseek(f,0L,SEEK_END)!=0||(sz=ftell(f))<58L||(unsigned long)sz>CAP||fseek(f,0L,SEEK_SET)!=0){fclose(f);return 3;}
    if(fread(raw,1U,(size_t)sz,f)!=(size_t)sz){fclose(f);return 3;}fclose(f);
    src.data=raw;src.size=(re4_size)sz;
    re4_arena_init(&a,pool,POOL);
    err=re4_pmd_read_survey(src,&a,&model);
    if(err!=RE4_OK){printf("PARSE_ERROR,%s\n",re4_err_name(err));return 4;}
    for(i=0;i<model.node_count;++i){
        const re4_pmd_node *node=&model.nodes[i];
        for(j=0;j<node->node_bone_count;++j){
            re4_size at=(re4_size)node->node_bone_offset+(re4_size)j*72UL;
            re4_i32 bone;
            if(!re4_span_ok(src,at,72UL)){++bad_bone_count;continue;}
            bone=re4_read_i32_e(src.data+at,RE4_ENDIAN_LITTLE);
            if(bone<0||(re4_u32)bone>=model.skeleton_count){++bad_bone_count;raw_invalid_bone_id=(re4_u32)bone;}
        }
        for(j=0;j<node->mesh_count;++j){
            const re4_pmd_mesh *mesh=&node->meshes[j];
            for(k=0;k<mesh->vertex_count;++k)check_vertex(src,node,mesh,i,k);
            for(k=0;k<mesh->index_count/3U;++k){
                re4_pmd_triangle tri;
                err=re4_pmd_mesh_triangle(src,mesh,k,&tri);
                if(err!=RE4_OK||tri.vertex[0]>=mesh->vertex_count||tri.vertex[1]>=mesh->vertex_count||tri.vertex[2]>=mesh->vertex_count)++bad_triangle_count;
                else if(tri.vertex[0]==tri.vertex[1]||tri.vertex[0]==tri.vertex[2]||tri.vertex[1]==tri.vertex[2])++zero_triangle_count;
            }
        }
    }
    error_count=read_errors+negative_weight_count+zero_sum_count+bad_slot_count+bad_bone_count+bad_triangle_count;
    printf("%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
      error_count?"FLAG":"OK",(unsigned long)model.node_count,(unsigned long)model.skeleton_count,
      (unsigned long)vertex_count,(unsigned long)rigid_count,(unsigned long)skinned_count,
      (unsigned long)read_errors,(unsigned long)negative_weight_count,
      (unsigned long)zero_sum_count,(unsigned long)near_weight_count,
      (unsigned long)bad_slot_count,(unsigned long)bad_bone_count,
      (unsigned long)bad_triangle_count,(unsigned long)zero_triangle_count,
      (unsigned long)(min_weight==4294967295UL?0:min_weight),
      (unsigned long)max_weight,(unsigned long)max_deviation,
      (unsigned long)extreme_positions,
      (unsigned long)(bad_slot_example_node==4294967295UL?0:bad_slot_example_node),
      (unsigned long)bad_slot_example_vertex,
      (unsigned long)bad_slot_example_index);
    return error_count?1:0;
}
