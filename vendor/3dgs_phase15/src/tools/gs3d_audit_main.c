#include "../../include/gs3d.h"
#include <stdio.h>
int main(int argc,char **argv){
    GS3D_FileSections sections;GS3D_DeformerInfo di;GS3D_MaterialInfo mi;
    unsigned int i,j;int r;
    if(argc!=2){fputs("usage: gs3d_audit model.mdl\n",stderr);return 2;}
    r=gs3d_open(argv[1]);
    if(r){fprintf(stderr,"parse_error=%s (%d)\n",gs3d_error_name(r),r);return 1;}
    gs3d_get_file_sections(&sections);
    printf("format=MDL%d size=%lu groups=%u bones=%u verts=%lu triangles=%lu\n",
           gs3d_model.format,gs3d_model.file_size,gs3d_model.groups,gs3d_model.bones,
           gs3d_model.vertices,gs3d_model.faces);
    if(gs3d_model.format==7){
        printf("mdl_payload=%lu ent_lump=%lu med_lump=%lu trailing=%lu\n",
               sections.mdl_data_end,sections.entity_lump_size,
               sections.med_lump_size,sections.trailing_bytes);
    }
    for(i=0U;i<gs3d_model.groups;i++){
        GS3D_Group *g=&gs3d_model.group[i];
        gs3d_get_deformer_info(i,&di);
        printf("group=%u name=%s vertices=%lu faces=%lu uvs=%lu skins=%u frames=%u vertex_stride=%u deformers=%u/%u max_weights=%u influences=%lu weighted_vertices=%lu\n",
             i,g->name,g->vertex_count,g->face_count,g->uv_count,g->skin_count,g->frame_count,
             g->vertex_stride,di.parsed_deformers,di.declared_deformers,
             di.declared_max_weights,di.explicit_influences,di.weighted_vertices);
        for(j=0U;j<g->skin_count;j++){
            gs3d_get_material(i,j,&mi);
            printf(" skin=%u color=%d image=%d diffuse_q16=(%ld,%ld,%ld) opacity=%ld\n",
                j,(int)mi.is_explicit,(int)mi.has_image,mi.diffuse[0],mi.diffuse[1],
                mi.diffuse[2],mi.opacity);
        }
    }
    gs3d_close();return 0;
}
