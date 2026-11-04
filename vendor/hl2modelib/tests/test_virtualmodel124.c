/* v124 recursive includemodel graph. Strict C89, no assets required. */
#include "hl2modelib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static hl2m_mdl models[4];
static hl2m_u8 bytes[4][1024];
static hl2m_virtual_input123 inputs[4];
static hl2m_virtual_multi123 catalog,unchanged;
static hl2m_virtual_graph124 graph,oldgraph;
static void chk(int okay,const char *s)
{if(!okay){fprintf(stderr,"V124 FAIL: %s\n",s);exit(1);}}
static void put32(hl2m_u8 *p,unsigned int a,unsigned int v)
{p[a]=(hl2m_u8)v;p[a+1]=(hl2m_u8)(v>>8);
 p[a+2]=(hl2m_u8)(v>>16);p[a+3]=(hl2m_u8)(v>>24);}
static void inc(int i,const char *a,const char *b)
{
    put32(bytes[i],336,a?(b?2u:1u):0u);
    if(!a)return;
    put32(bytes[i],340,400u);
    put32(bytes[i],404,48u);strcpy((char*)bytes[i]+448,a);
    if(b){put32(bytes[i],412,72u);strcpy((char*)bytes[i]+480,b);}
}
static void reset(void)
{
    int i;
    memset(models,0,sizeof(models));memset(bytes,0,sizeof(bytes));
    memset(inputs,0,sizeof(inputs));
    for(i=0;i<4;i++){
        memcpy(bytes[i],"IDST",4);
        inputs[i].mdl_bytes=bytes[i];inputs[i].mdl_size=1024u;
        inputs[i].mdl=&models[i];models[i].bone_count=1;
        strcpy(models[i].bones[0].name,"pelvis");models[i].bones[0].parent=-1;
    }
    /* Out-of-order binding: base, C, B, A. Graph DFS = base,A,B,C. */
    inputs[1].include_path="models\\C.mdl";
    inputs[2].include_path="models/B.mdl";
    inputs[3].include_path="models/a.mdl";
    inc(0,"models/a.mdl","models/C.mdl");
    inc(3,"models/b.mdl",NULL);
    inc(1,"models/b.mdl",NULL); /* diamond, do not duplicate B */
    models[0].sequence_count=1;
    strcpy(models[0].sequences[0].label,"idle");
    models[0].sequences[0].flags=0x0800; /* Source forward override */
    models[3].sequence_count=2;
    strcpy(models[3].sequences[0].label,"IDLE");
    strcpy(models[3].sequences[1].label,"pulse");
    models[3].sequences[1].flags=0x0800;/* B overrides pulse */
    models[2].sequence_count=2;
    strcpy(models[2].sequences[0].label,"PULSE");
    strcpy(models[2].sequences[1].label,"wave");
    models[1].sequence_count=1;
    strcpy(models[1].sequences[0].label,"run");
}
int main(void)
{
    int r,target=-9;
    reset();
    chk(hl2m_virtual_build_multi123(inputs,4u,&catalog)==HL2M_ERR_RANGE,
        "v123 correctly rejects nested include graph");
    r=hl2m_virtual_build_recursive124(inputs,4u,&catalog,&graph);
    chk(r==HL2M_OK,"recursive graph builds");
    chk(catalog.group_count==4&&catalog.seq_count==4,"deduplicated catalogs");
    chk(graph.group_to_input[0]==0&&graph.group_to_input[1]==3&&
        graph.group_to_input[2]==2&&graph.group_to_input[3]==1,
        "depth-first, ordered base A B C");
    chk(graph.parent_group[0]==-1&&graph.parent_group[1]==0&&
        graph.parent_group[2]==1&&graph.parent_group[3]==0,
        "source-style first owner of shared bank");
    chk(catalog.sequences[0].group==1&&catalog.sequences[0].local==0,
        "A replaces base forward idle");
    chk(catalog.sequences[1].group==2&&catalog.sequences[1].local==0,
        "B replaces A forward pulse");
    chk(hl2m_virtual_layer_target124(&catalog,0,1,&target)==HL2M_OK&&
        target==1,"relative layer ID resolves from A to B override");
    chk(hl2m_virtual_layer_target124(&catalog,0,300,&target)==HL2M_ERR_RANGE&&
        target==1,"invalid layer is transactional");
    unchanged=catalog;oldgraph=graph;
    inc(2,"models/A.mdl",NULL);
    chk(hl2m_virtual_build_recursive124(inputs,4u,&catalog,&graph)==HL2M_ERR_PARSE,
        "cycle base->A->B->A rejected");
    chk(!memcmp(&unchanged,&catalog,sizeof(catalog))&&
        !memcmp(&oldgraph,&graph,sizeof(graph)),"cycle does not alter output");
    inc(2,NULL,NULL);
    inc(3,"models/no_such.mdl",NULL);
    chk(hl2m_virtual_build_recursive124(inputs,4u,&catalog,&graph)==HL2M_ERR_RANGE,
        "missing dependency rejected");
    inc(3,"models/b.mdl",NULL);
    inc(0,"models/a.mdl",NULL);
    chk(hl2m_virtual_build_recursive124(inputs,4u,&catalog,&graph)==HL2M_ERR_RANGE,
        "orphan supplied C rejected");
    inc(0,"models/a.mdl","models/C.mdl");
    inputs[2].include_path="MODELS\\A.MDL";
    chk(hl2m_virtual_build_recursive124(inputs,4u,&catalog,&graph)==HL2M_ERR_PARSE,
        "duplicate input paths rejected");
    puts("V124 PASS: nested include, diamond, order, override, layer targets, rejects");
    return 0;
}
