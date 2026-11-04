#include <stdio.h>
#include <string.h>
#include "xp.h"

typedef struct sem_probe {
    unsigned int root_rel;
    unsigned int bone_rel;
    unsigned int root_node;
    unsigned int bone_node;
    unsigned int mesh_node_bind;
    unsigned int mesh_bind;
    unsigned int skin_bind;
    unsigned int inv_bind;
    unsigned int uv_seen;
    unsigned int wrap_seen;
    unsigned int face_wrap_seen;
    unsigned int target_seen;
    unsigned int legacy_matrix_keys;
    unsigned int summary_seen;
} sem_probe;

static void frame_rel(void* u, const char* n, const char* p, unsigned int d) {
    sem_probe* s=(sem_probe*)u;
    if (strcmp(n,"Root")==0 && strcmp(p,"")==0 && d==0u) s->root_rel++;
    if (strcmp(n,"Bone")==0 && strcmp(p,"Root")==0 && d==1u) s->bone_rel++;
}
static void frame_node(void* u, unsigned int id, unsigned int parent, const char* n, unsigned int d) {
    sem_probe* s=(sem_probe*)u;
    if (strcmp(n,"Root")==0 && id==0u && parent==XP_FRAME_ID_NONE && d==0u) s->root_node++;
    if (strcmp(n,"Bone")==0 && id==1u && parent==0u && d==1u) s->bone_node++;
}
static void mesh_node_bind(void* u, const char* m, unsigned int id) {
    sem_probe* s=(sem_probe*)u;
    if (strcmp(m,"RigMesh")==0 && id==0u) s->mesh_node_bind++;
}
static void mesh_bind(void* u, const char* m, const char* f) {
    sem_probe* s=(sem_probe*)u;
    if (strcmp(m,"RigMesh")==0 && strcmp(f,"Root")==0) s->mesh_bind++;
}
static void skin_bind(void* u, const char* m, const char* b, int n) {
    sem_probe* s=(sem_probe*)u;
    if (strcmp(m,"RigMesh")==0 && strcmp(b,"Bone")==0 && n==3) s->skin_bind++;
}
static void inv_bind(void* u, const char* m, const char* b, const xp_fx32 x[16]) {
    sem_probe* s=(sem_probe*)u;
    if (strcmp(m,"RigMesh")==0 && strcmp(b,"Bone")==0 && x[0]==XP_FX_ONE &&
        x[5]==XP_FX_ONE && x[10]==XP_FX_ONE && x[15]==XP_FX_ONE && x[12]==0) s->inv_bind++;
}
static void uv(void* u, int i, xp_fx32 x, xp_fx32 y) {
    sem_probe* s=(sem_probe*)u;
    if (i==0 && x>(xp_fx32)(2*XP_FX_ONE) && y<0) s->uv_seen++;
}
static void material_wrap(void* u, unsigned int a, unsigned int b) {
    sem_probe* s=(sem_probe*)u;
    if (a==1u && b==0u) s->wrap_seen++;
}
static void face_wrap(void* u, unsigned int i, unsigned int a, unsigned int b) {
    sem_probe* s=(sem_probe*)u;
    if (i==0u && a==1u && b==0u) s->face_wrap_seen++;
}
static void target(void* u, const char* n) {
    sem_probe* s=(sem_probe*)u;
    if (strcmp(n,"Bone")==0) s->target_seen++;
}
static void sem_key(void* u, int raw, xp_anim_key_semantic sem, unsigned int flags,
                    int ki, int time, int nv, const xp_fx32* values) {
    sem_probe* s=(sem_probe*)u;
    (void)values;
    if (raw==4 && sem==XP_ANIM_SEM_MATRIX && nv==16 &&
        (flags & XP_ANIM_KEY_FLAG_LEGACY_TYPE4)!=0u &&
        (flags & XP_ANIM_KEY_FLAG_ABSOLUTE_LOCAL)!=0u &&
        ((ki==0 && time==0) || (ki==1 && time==10))) s->legacy_matrix_keys++;
}
static void summary(void* u, const char* n, unsigned int tracks, int first, int last,
                    unsigned int tps, int known) {
    sem_probe* s=(sem_probe*)u;
    if (strcmp(n,"Clip")==0 && tracks==1u && first==0 && last==10 && tps==0u && known==0)
        s->summary_seen++;
}

static int helper_test(void) {
    xp_fx32 a[16], b[16], c[16], skin[16];
    xp_fx32 x,y,z,u,v;
    unsigned int flags;
    xp_anim_key_semantic sem;
    xp_mat4_identity(a);
    xp_mat4_identity(b);
    a[12]=(xp_fx32)(2*XP_FX_ONE);
    b[13]=(xp_fx32)(3*XP_FX_ONE);
    xp_mat4_mul_dx(a,b,c);
    if (c[12]!=(xp_fx32)(2*XP_FX_ONE) || c[13]!=(xp_fx32)(3*XP_FX_ONE)) return 0;
    xp_skin_final_matrix(a,b,skin);
    if (skin[12]!=(xp_fx32)(2*XP_FX_ONE) || skin[13]!=(xp_fx32)(3*XP_FX_ONE)) return 0;
    xp_transform_point_dx(c,XP_FX_ONE,XP_FX_ONE,0,&x,&y,&z);
    if (x!=(xp_fx32)(3*XP_FX_ONE) || y!=(xp_fx32)(4*XP_FX_ONE) || z!=0) return 0;
    xp_uv_dx_to_gl((xp_fx32)147456,(xp_fx32)-32768,&u,&v);
    if (u!=(xp_fx32)147456 || v!=(xp_fx32)98304) return 0;
    flags=0u;
    sem=xp_animation_key_semantic_kind(4,16,&flags);
    if (sem!=XP_ANIM_SEM_MATRIX || (flags & XP_ANIM_KEY_FLAG_LEGACY_TYPE4)==0u) return 0;
    return 1;
}

int main(void) {
    FILE* f;
    xp_context ctx;
    xp_header hdr;
    xp_callbacks cb;
    sem_probe s;
    xp_result r;
    memset(&cb,0,sizeof(cb));
    memset(&s,0,sizeof(s));
    cb.on_frame_relation=frame_rel;
    cb.on_frame_node_begin=frame_node;
    cb.on_mesh_frame_binding=mesh_bind;
    cb.on_mesh_frame_node_binding=mesh_node_bind;
    cb.on_skin_binding_begin=skin_bind;
    cb.on_skin_inverse_bind_matrix=inv_bind;
    cb.on_texcoord=uv;
    cb.on_material_wrap=material_wrap;
    cb.on_mesh_face_wrap=face_wrap;
    cb.on_animation_target=target;
    cb.on_animation_key_semantic=sem_key;
    cb.on_animation_set_summary=summary;
    f=fopen("tests/fixtures/runtime_v04_rig_uv_anim_text.x","rb");
    if (!f) return 10;
    r=xp_open_file(&ctx,f,&hdr);
    if (r!=XP_OK) { fclose(f); return 11; }
    r=xp_parse(&ctx,&cb,&s);
    fclose(f);
    if (r!=XP_OK) return 12;
    if (s.root_rel!=1u || s.bone_rel!=1u || s.root_node!=1u || s.bone_node!=1u ||
        s.mesh_node_bind!=1u || s.mesh_bind!=1u || s.skin_bind!=1u ||
        s.inv_bind!=1u || s.uv_seen!=1u || s.wrap_seen!=1u || s.face_wrap_seen!=1u ||
        s.target_seen!=1u || s.legacy_matrix_keys!=2u || s.summary_seen!=1u) return 20;
    if (!helper_test()) return 21;
    puts("v0.4 rig/uv/animation semantics: OK");
    return 0;
}
