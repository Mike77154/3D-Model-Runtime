#include "3dfbx.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    static unsigned char out[131072];
    fbx_real pos[12];
    fbx_real nrm[12];
    fbx_real uv[8];
    fbx_real colors[16];
    fbx_u32 idx[6];
    FbxSemanticMesh mesh;
    FbxSemanticScene sem_scene;
    FbxSemanticWriteOptions opt;
    FbxWriteBuffer wb;
    FbxCodecDecodeResult decoded;
    int ok;

    pos[0] = -FBX_REAL_ONE; pos[1] = 0; pos[2] = 0;
    pos[3] =  FBX_REAL_ONE; pos[4] = 0; pos[5] = 0;
    pos[6] =  FBX_REAL_ONE; pos[7] = FBX_REAL_ONE; pos[8] = 0;
    pos[9] = -FBX_REAL_ONE; pos[10] = FBX_REAL_ONE; pos[11] = 0;

    nrm[0] = 0; nrm[1] = 0; nrm[2] = FBX_REAL_ONE;
    nrm[3] = 0; nrm[4] = 0; nrm[5] = FBX_REAL_ONE;
    nrm[6] = 0; nrm[7] = 0; nrm[8] = FBX_REAL_ONE;
    nrm[9] = 0; nrm[10] = 0; nrm[11] = FBX_REAL_ONE;

    uv[0] = 0; uv[1] = 0;
    uv[2] = FBX_REAL_ONE; uv[3] = 0;
    uv[4] = FBX_REAL_ONE; uv[5] = FBX_REAL_ONE;
    uv[6] = 0; uv[7] = FBX_REAL_ONE;

    colors[0] = FBX_REAL_ONE; colors[1] = 0; colors[2] = 0; colors[3] = FBX_REAL_ONE;
    colors[4] = 0; colors[5] = FBX_REAL_ONE; colors[6] = 0; colors[7] = FBX_REAL_ONE;
    colors[8] = 0; colors[9] = 0; colors[10] = FBX_REAL_ONE; colors[11] = FBX_REAL_ONE;
    colors[12] = FBX_REAL_ONE; colors[13] = FBX_REAL_ONE; colors[14] = FBX_REAL_ONE; colors[15] = FBX_REAL_ONE;

    idx[0] = 0u; idx[1] = 1u; idx[2] = 2u; idx[3] = 0u; idx[4] = 2u; idx[5] = 3u;

    memset(&mesh, 0, sizeof(mesh));
    mesh.name = "SemanticQuad";
    mesh.positions = pos;
    mesh.normals = nrm;
    mesh.uvs = uv;
    mesh.colors = colors;
    mesh.vertex_count = 4u;
    mesh.indices = idx;
    mesh.index_count = 6u;
    mesh.material_name = "SemanticMat";
    mesh.diffuse_texture = "semantic_diffuse.png";

    memset(&sem_scene, 0, sizeof(sem_scene));
    sem_scene.creator = "semantic writer smoke";
    sem_scene.meshes = &mesh;
    sem_scene.mesh_count = 1;

    fbx_semantic_write_options_default(&opt);
    opt.version = 7500;
    opt.binary_compress_arrays = 1;

    fbx_write_buffer_init(&wb, out, sizeof(out));
    if (fbx_semantic_write_binary(&sem_scene, &wb, &opt) != FBX_OK) return 1;

    fbx_codec_decode_result_init(&decoded);
    ok = fbx_codec_decode_memory(out, (fbx_i64)wb.size, 1, &decoded);
    if (!ok) return 2;
    if (!decoded.scene || decoded.scene->model_count != 1 || decoded.scene->material_count != 1) return 3;
    fbx_codec_decode_close(&decoded);

    printf("semantic writer OK: bytes=%lu\n", (unsigned long)wb.size);
    return 0;
}
