/* MS3D 1.8.5 file audit; C89, no heap, no floats, no 64-bit integers. */
#include "gif_ms3d_codec.h"
#include <stdio.h>
#define FILE_CAP (16UL*1024UL*1024UL)
static unsigned char data[FILE_CAP];
static ms3d_model m;
int main(int argc,char **argv)
{
    FILE *f;size_t got;int rc,next;
    ms3d_parse_info info;
    unsigned i,j;
    unsigned bad_rot=0U,bad_pos=0U,group_oob=0U,weighted_vertices=0U;
    if(argc!=2){fputs("usage: ms3d_spec_audit file.ms3d\n",stderr);return 2;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    got=fread(data,1U,sizeof(data),f);next=fgetc(f);fclose(f);
    if(got==0U||next!=EOF){fputs("oversize/empty\n",stderr);return 2;}
    rc=ms3d_parse_ex(&m,data,got,&info);
    if(rc!=MS3D_OK){fprintf(stderr,"parse failed (%d): %s\n",rc,ms3d_error_string(rc));return 1;}
    for(i=0U;i<m.num_joints;++i){
        const ms3d_joint *joint=&m.joints[i];
        for(j=1U;j<joint->rot_key_count;++j){
            unsigned a=(unsigned)joint->rot_key_start+j;
            if(m.rot_keys[a].time<m.rot_keys[a-1U].time)++bad_rot;
        }
        for(j=1U;j<joint->pos_key_count;++j){
            unsigned a=(unsigned)joint->pos_key_start+j;
            if(m.pos_keys[a].time<m.pos_keys[a-1U].time)++bad_pos;
        }
    }
    for(i=0U;i<m.num_triangles;++i)
        if(m.triangles[i].group_index>=m.num_groups&&m.num_groups>0U)++group_oob;
    for(i=0U;i<m.num_vertices;++i)
        if(m.vertex_extra_subversion>0L&&(m.vertices[i].weights[0]||m.vertices[i].weights[1]||m.vertices[i].weights[2]))
            ++weighted_vertices;
    printf("file=%s\nvertices=%u\ntriangles=%u\ngroups=%u\nmaterials=%u\njoints=%u\n",
           argv[1],(unsigned)m.num_vertices,(unsigned)m.num_triangles,(unsigned)m.num_groups,
           (unsigned)m.num_materials,(unsigned)m.num_joints);
    printf("rotation_keys=%u\nposition_keys=%u\n",(unsigned)m.num_rot_keys,(unsigned)m.num_pos_keys);
    printf("comments_version=%ld\nvertex_extra_version=%ld\njoint_extra_version=%ld\nmodel_extra_version=%ld\n",
           m.comments_subversion,m.vertex_extra_subversion,m.joint_extra_subversion,m.model_extra_subversion);
    printf("weighted_vertices=%u\nbackwards_rotation_key_intervals=%u\nbackwards_position_key_intervals=%u\n",
           weighted_vertices,bad_rot,bad_pos);
    printf("invalid_triangle_group_hints=%u\nbytes_used=%lu\nbytes_remaining=%lu\n",
           group_oob,(unsigned long)info.bytes_used,(unsigned long)info.bytes_remaining);
    return 0;
}
