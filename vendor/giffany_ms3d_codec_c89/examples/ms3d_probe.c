/* Non-rendering asset probe. ANSI C89, fixed point, no heap. */
#include "gif_ms3d_codec.h"
#include <stdio.h>

#define INPUT_MAX (16UL * 1024UL * 1024UL)
static unsigned char g_input[INPUT_MAX];
static ms3d_model g_model;

int main(int argc, char **argv)
{
    FILE *f;
    size_t total;
    size_t got;
    ms3d_u16 i, j;
    ms3d_u8 n;
    int rc;
    unsigned long active, animated, bad_weight;
    ms3d_fx time,rot[3],pos[3],sum;
    ms3d_influence inf[4];
    ms3d_parse_info info;
    if (argc != 2) { fputs("usage: ms3d_probe model.ms3d\n", stderr); return 2; }
    f = fopen(argv[1], "rb");
    if (f == (FILE *)0) { fputs("open failure\n", stderr); return 2; }
    total=0U;
    while (total < INPUT_MAX) {
        got=fread(g_input+total,1U,INPUT_MAX-total,f);
        total+=got;
        if (got == 0U) break;
    }
    if (fgetc(f) != EOF || total == 0U) { fclose(f); fputs("input too large/empty\n", stderr); return 2; }
    fclose(f);
    rc=ms3d_parse_ex(&g_model,g_input,total,&info);
    if (rc != MS3D_OK) {
        fprintf(stderr,"FAIL parse %s (%d)\n",ms3d_error_string(rc),rc);
        return 1;
    }
    active=0UL;bad_weight=0UL;animated=0UL;
    for (i=0U;i<g_model.num_vertices;++i) {
        rc=ms3d_vertex_get_influences(&g_model,i,inf,&n);
        if (rc != MS3D_OK) { fputs("FAIL influences\n",stderr); return 1; }
        if (n!=0U) {
            ++active;sum=0L;
            for (j=0U;j<n;++j) {
                if (inf[j].bone_id < 0 || inf[j].bone_id >= (int)g_model.num_joints) {
                    fputs("FAIL bone index\n",stderr); return 1;
                }
                sum=ms3d_fx_add_sat(sum,inf[j].weight);
            }
            if (sum > MS3D_FX_ONE+4L) ++bad_weight;
        }
    }
    for (i=0U;i<g_model.num_joints;++i) {
        time=0L;
        if (g_model.joints[i].rot_key_count) {
            ms3d_u16 last;
            last=(ms3d_u16)(g_model.joints[i].rot_key_start+g_model.joints[i].rot_key_count-1U);
            time=g_model.rot_keys[last].time;
        }
        if (g_model.joints[i].pos_key_count) {
            ms3d_u16 last;
            last=(ms3d_u16)(g_model.joints[i].pos_key_start+g_model.joints[i].pos_key_count-1U);
            if (g_model.pos_keys[last].time > time) time=g_model.pos_keys[last].time;
        }
        rc=ms3d_sample_joint_local(&g_model,i,time,rot,pos);
        if (rc != MS3D_OK) { fputs("FAIL sampling\n",stderr); return 1; }
        if (g_model.joints[i].rot_key_count || g_model.joints[i].pos_key_count) ++animated;
        /* A cycle would make hierarchical pose evaluation non-terminating. */
        {
            int parent,depth;
            parent=g_model.joints[i].parent_index;depth=0;
            while (parent>=0 && depth <= (int)g_model.num_joints) {
                if (parent >= (int)g_model.num_joints) { fputs("FAIL parent index\n",stderr); return 1; }
                parent=g_model.joints[parent].parent_index;
                ++depth;
            }
            if (depth > (int)g_model.num_joints) { fputs("FAIL parent cycle\n",stderr); return 1; }
        }
    }
    if (bad_weight) { fprintf(stderr,"FAIL %lu invalid total weights\n",bad_weight); return 1; }
    printf("PASS vertices=%u triangles=%u groups=%u materials=%u joints=%u rot_keys=%u pos_keys=%u skinned=%lu animated_joints=%lu comment_ext=%ld vertex_ext=%ld bytes_remaining=%lu\n",
       (unsigned)g_model.num_vertices,(unsigned)g_model.num_triangles,
       (unsigned)g_model.num_groups,(unsigned)g_model.num_materials,
       (unsigned)g_model.num_joints,(unsigned)g_model.num_rot_keys,
       (unsigned)g_model.num_pos_keys,active,animated,g_model.comments_subversion,
       g_model.vertex_extra_subversion,(unsigned long)info.bytes_remaining);
    return 0;
}
