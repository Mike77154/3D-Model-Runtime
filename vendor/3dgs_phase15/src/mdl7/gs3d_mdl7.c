#include "../gs3d_internal.h"
/* Group metadata + stage delegation. */
int gs7_group(unsigned long *cursor,unsigned long index){
    unsigned char b[44];unsigned long cur=*cursor,group_end,record_bytes;
    unsigned long ni,nuv,ntri,nv,nf,j;int r;GS3D_Group *g=&gs3d_model.group[index];
    if(gs_get(cur,b,44UL))return GS3D_ERR_RANGE;
    g->group_type=b[0];record_bytes=gs_u32(b+4);
    /* Deformer tables are a separate post-group block in MED's MDL7.
       Preserve declaration now; consume the records after ALL groups. */
    gs_deformer_info[index].declared_deformers=b[1];
    gs_deformer_info[index].declared_max_weights=b[2];
    gs_deformer_info[index].kind=gs3d_model.bones ? GS3D_DEFORM_RIGID_BONE:GS3D_DEFORM_NONE;
    if(g->group_type!=1U)return GS3D_ERR_UNSUPPORTED;
    if(record_bytes<44UL)return GS3D_ERR_CORRUPT;
    if(gs_addmul(cur,1UL,record_bytes,&group_end))return GS3D_ERR_RANGE;
    if(group_end>gs_file_sections.mdl_data_end)return GS3D_ERR_RANGE;
    memcpy(g->name,b+8,16U);g->name[16]='\0';
    ni=gs_u32(b+24);nuv=gs_u32(b+28);ntri=gs_u32(b+32);
    nv=gs_u32(b+36);nf=gs_u32(b+40);
    if(ni>GS3D_MAX_SKINS||nf>GS3D_MAX_FRAMES||
       nv>GS3D_MAX_VERTS-gs3d_model.vertices||
       nuv>GS3D_MAX_UVS-gs3d_model.uvs||
       ntri>GS3D_MAX_FACES-gs3d_model.faces)return GS3D_ERR_CAPACITY;
    g->skin_count=(unsigned short)ni;g->frame_count=(unsigned short)nf;
    g->vertex_count=nv;g->uv_count=nuv;g->face_count=ntri;
    g->vertex_first=gs3d_model.vertices;g->uv_first=gs3d_model.uvs;
    g->face_first=gs3d_model.faces;
    g->vertex_stride=gs7_header_stride[6];g->frame_stride=gs7_header_stride[7];
    gs3d_model.vertices+=nv;gs3d_model.uvs+=nuv;gs3d_model.faces+=ntri;
    cur+=44UL;
    for(j=0UL;j<ni;j++){r=gs7_skin(&cur,group_end,&g->skins[j]);if(r)return r;}
    r=gs7_geometry(g,&cur,group_end,(unsigned long)gs7_header_stride[4],
            (unsigned long)gs7_header_stride[5],(unsigned long)gs7_header_stride[6]);
    if(r)return r;
    r=gs7_frames(g,&cur,group_end);if(r)return r;
    if(cur>group_end)return GS3D_ERR_RANGE;
    /* Some synthetic/older converters place records INSIDE groupdata_size.
       Real Tusker places them after ALL groups. Accept both, but validate. */
    if(cur<group_end && b[1]){
        r=gs7_read_deformers(g,(unsigned int)index,&cur,group_end,
                   (unsigned int)b[1],(unsigned int)b[2]);
        if(r)return r;
    }
    *cursor=group_end;return GS3D_OK;
}
int gs_mdl7(void){
    unsigned long cur=0UL,i;int r;
    r=gs7_header(&cur);if(r)return r;
    r=gs7_bones(&cur);if(r)return r;
    for(i=0UL;i<gs3d_model.groups;i++){r=gs7_group(&cur,i);if(r)return r;}
    /* Authentic Tusker.MDL stores its 49,516-byte deformer table after
       groupdata_size, but before data_size. Do not read it as a MED lump. */
    for(i=0UL;i<gs3d_model.groups;i++){
        GS3D_DeformerInfo *di=&gs_deformer_info[i];
        if(di->parsed_deformers<di->declared_deformers){
            r=gs7_read_deformers(&gs3d_model.group[i],(unsigned int)i,
                      &cur,gs_file_sections.mdl_data_end,
                      (unsigned int)(di->declared_deformers-di->parsed_deformers),
                      (unsigned int)di->declared_max_weights);
            if(r)return r;
        }
    }
    /* Reject unaccounted data bytes: MED belongs to a separate lump. */
    if(cur!=gs_file_sections.mdl_data_end)return GS3D_ERR_UNSUPPORTED;
    return GS3D_OK;
}
