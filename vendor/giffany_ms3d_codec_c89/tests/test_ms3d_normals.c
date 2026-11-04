/* MS3D smoothing group regression: shared edge, intentional hard edge.
 * No heap; static models to match engine constraints.
 */
#include "gif_ms3d_normals.h"
#include <stdio.h>
#include <string.h>

static ms3d_model m;
static ms3d_corner_normals n;
static unsigned g_corners;
static int count_corner(void *user, ms3d_u16 tri, ms3d_u8 corner, const ms3d_draw_corner *v)
{
    (void)user; (void)tri; (void)corner;
    if (v->normal[0] == 314159L) return MS3D_E_BAD_INDEX;
    ++g_corners;
    return MS3D_OK;
}
#define CHECK(t) do { if (!(t)) { puts("FAIL " #t); return 1; } } while(0)
static void setvertex(int i, int x, int y, int z)
{
    m.vertices[i].position[0] = ms3d_fx_from_int(x);
    m.vertices[i].position[1] = ms3d_fx_from_int(y);
    m.vertices[i].position[2] = ms3d_fx_from_int(z);
}
int main(void)
{
    int rc;
    ms3d_shaded_sink sink;
    memset(&m, 0, sizeof(m));
    m.num_vertices = 4U;
    m.num_triangles = 2U;
    setvertex(0, 0, 0, 0);
    setvertex(1, 1, 0, 0);
    setvertex(2, 0, 1, 0);
    setvertex(3, 0, 0, 1);
    m.triangles[0].vertex_indices[0] = 0U;
    m.triangles[0].vertex_indices[1] = 1U;
    m.triangles[0].vertex_indices[2] = 2U;
    m.triangles[1].vertex_indices[0] = 0U;
    m.triangles[1].vertex_indices[1] = 3U;
    m.triangles[1].vertex_indices[2] = 1U;
    m.triangles[0].smoothing_group = 1U;
    m.triangles[1].smoothing_group = 1U;
    rc = ms3d_build_corner_normals(&m, &n, MS3D_NORMAL_SMOOTH_GROUP);
    CHECK(rc == MS3D_OK);
    CHECK(n.xyz[0][0][1] > 40000L && n.xyz[0][0][2] > 40000L);
    CHECK(n.xyz[1][0][1] == n.xyz[0][0][1]);
    CHECK(n.xyz[1][0][2] == n.xyz[0][0][2]);
    CHECK(n.xyz[0][2][1] == 0L && n.xyz[0][2][2] >= 65000L);
    m.triangles[1].smoothing_group = 2U;
    rc = ms3d_build_corner_normals(&m, &n, MS3D_NORMAL_SMOOTH_GROUP);
    CHECK(rc == MS3D_OK);
    CHECK(n.xyz[0][0][2] >= 65000L && n.xyz[0][0][1] == 0L);
    CHECK(n.xyz[1][0][1] >= 65000L && n.xyz[1][0][2] == 0L);
    rc = ms3d_build_corner_normals(&m, &n, MS3D_NORMAL_SMOOTH_ALL);
    CHECK(rc == MS3D_OK);
    CHECK(n.xyz[0][0][1] > 40000L && n.xyz[0][0][2] > 40000L);
    rc = ms3d_build_corner_normals(&m, &n, MS3D_NORMAL_FLAT);
    CHECK(rc == MS3D_OK);
    CHECK(n.xyz[0][0][1] == 0L && n.xyz[0][0][2] >= 65000L);
    CHECK(n.xyz[1][0][1] >= 65000L && n.xyz[1][0][2] == 0L);
    /* zero means explicit hard face, even next to smooth group 1 */
    m.triangles[1].smoothing_group = 0U;
    rc = ms3d_build_corner_normals(&m, &n, MS3D_NORMAL_SMOOTH_GROUP);
    CHECK(rc == MS3D_OK);
    CHECK(n.xyz[0][0][1] == 0L && n.xyz[0][0][2] >= 65000L);
    CHECK(n.xyz[1][0][1] >= 65000L && n.xyz[1][0][2] == 0L);
    /* Duplicate vertex at same position: preserve UV seam, weld normals. */
    m.num_vertices = 5U;
    setvertex(4, 0, 0, 0);
    m.triangles[1].vertex_indices[0] = 4U;
    m.triangles[1].smoothing_group = 1U;
    rc = ms3d_build_corner_normals(&m, &n, MS3D_NORMAL_SMOOTH_GROUP);
    CHECK(rc == MS3D_OK);
    CHECK(n.xyz[0][0][2] >= 65000L);
    CHECK(n.xyz[1][0][1] >= 65000L);
    rc = ms3d_build_corner_normals(&m, &n, MS3D_NORMAL_SMOOTH_WELDED);
    CHECK(rc == MS3D_OK);
    CHECK(n.xyz[0][0][1] > 40000L && n.xyz[0][0][2] > 40000L);
    CHECK(n.xyz[0][0][1] == n.xyz[1][0][1]);
    CHECK(n.xyz[0][0][2] == n.xyz[1][0][2]);
    /* Smoothing groups remain separate even when positions are coincident. */
    m.triangles[1].smoothing_group = 2U;
    rc = ms3d_build_corner_normals(&m, &n, MS3D_NORMAL_SMOOTH_WELDED);
    CHECK(rc == MS3D_OK);
    CHECK(n.xyz[0][0][2] >= 65000L && n.xyz[1][0][1] >= 65000L);
    sink.user = (void *)0;
    sink.corner = count_corner;
    g_corners = 0U;
    CHECK(ms3d_emit_shaded_corners(&m, &n, &sink) == MS3D_OK);
    CHECK(g_corners == 6U);
    puts("PASS synthetic normals (smooth/group/weld/flat) and sink");
    return 0;
}
