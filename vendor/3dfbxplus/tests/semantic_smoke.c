#include <stdio.h>
#include <string.h>
#include "3dfbx.h"

static int null_write(void *user, const void *data, size_t size)
{
    size_t *total;
    (void)data;
    total = (size_t*)user;
    *total += size;
    return 1;
}

int main(void)
{
    fbx_real pos[12];
    fbx_real nrm[12];
    fbx_real uv[8];
    fbx_real col[16];
    fbx_u32 idx[6];
    FbxSemanticMesh mesh;
    FbxSemanticScene scene;
    FbxSemanticWriteOptions opt;
    FbxCodecWriteStream stream;
    size_t bytes;
    int i;

    pos[0] = -FBX_REAL_ONE; pos[1] = FBX_REAL_ZERO; pos[2] = -FBX_REAL_ONE;
    pos[3] =  FBX_REAL_ONE; pos[4] = FBX_REAL_ZERO; pos[5] = -FBX_REAL_ONE;
    pos[6] =  FBX_REAL_ONE; pos[7] = FBX_REAL_ZERO; pos[8] =  FBX_REAL_ONE;
    pos[9] = -FBX_REAL_ONE; pos[10] = FBX_REAL_ZERO; pos[11] = FBX_REAL_ONE;
    for (i = 0; i < 4; ++i) {
        nrm[i * 3 + 0] = FBX_REAL_ZERO;
        nrm[i * 3 + 1] = FBX_REAL_ONE;
        nrm[i * 3 + 2] = FBX_REAL_ZERO;
        col[i * 4 + 0] = FBX_REAL_ONE;
        col[i * 4 + 1] = FBX_REAL_ONE;
        col[i * 4 + 2] = FBX_REAL_ONE;
        col[i * 4 + 3] = FBX_REAL_ONE;
    }
    uv[0] = FBX_REAL_ZERO; uv[1] = FBX_REAL_ZERO;
    uv[2] = FBX_REAL_ONE;  uv[3] = FBX_REAL_ZERO;
    uv[4] = FBX_REAL_ONE;  uv[5] = FBX_REAL_ONE;
    uv[6] = FBX_REAL_ZERO; uv[7] = FBX_REAL_ONE;
    idx[0] = 0u; idx[1] = 1u; idx[2] = 2u; idx[3] = 0u; idx[4] = 2u; idx[5] = 3u;

    memset(&mesh, 0, sizeof(mesh));
    memset(&scene, 0, sizeof(scene));

    mesh.name = "SemanticQuad";
    mesh.positions = pos;
    mesh.normals = nrm;
    mesh.uvs = uv;
    mesh.colors = col;
    mesh.vertex_count = 4u;
    mesh.indices = idx;
    mesh.index_count = 6u;
    mesh.material_name = "SemanticMaterial";
    mesh.diffuse_texture = "textures/semantic.png";
    scene.creator = "semantic smoke";
    scene.meshes = &mesh;
    scene.mesh_count = 1;
    fbx_semantic_write_options_default(&opt);
    bytes = 0u;
    stream.user = &bytes;
    stream.write = null_write;
    if (!fbx_semantic_write_ascii(&scene, &stream, &opt)) {
        printf("semantic ascii failed\n");
        return 2;
    }
    printf("semantic OK: ascii_bytes=%lu\n", (unsigned long)bytes);
    return 0;
}
