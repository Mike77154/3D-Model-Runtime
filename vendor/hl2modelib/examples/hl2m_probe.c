#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>

#define PROBE_BUFFER_SIZE (32u * 1024u * 1024u)
static unsigned char g_file_buffer[PROBE_BUFFER_SIZE];
static hl2m_mdl g_mdl;
static hl2m_vvd g_vvd;
static hl2m_vtx g_vtx;
static hl2m_smd g_smd;
static hl2m_vmt g_vmt;
static hl2m_phy g_phy;
static hl2m_ani g_ani;
static hl2m_vta g_vta;
static hl2m_qc g_qc;
static hl2m_material_desc g_mat;

static unsigned int load_file(const char *path)
{
    FILE *f;
    unsigned int n;
    size_t got;
    f = fopen(path, "rb");
    if (!f) return 0u;
    got = fread(g_file_buffer, 1u, PROBE_BUFFER_SIZE, f);
    fclose(f);
    n = (unsigned int)got;
    return n;
}

static int has_suffix(const char *s, const char *suffix)
{
    unsigned int a;
    unsigned int b;
    a = (unsigned int)strlen(s);
    b = (unsigned int)strlen(suffix);
    if (a < b) return 0;
    return strcmp(s + a - b, suffix) == 0;
}

static void print_fixed(hl2m_fixed v)
{
    char text[32];
    hl2m_fixed_to_string(v, text, (unsigned int)sizeof(text), 4);
    printf("%s", text);
}

static void print_mdl(void)
{
    int i;
    printf("MDL name=%s version=%d checksum=%d\n", g_mdl.name, g_mdl.version, g_mdl.checksum);
    printf("bones=%d/%d textures=%d/%d bodyparts=%d/%d models=%d meshes=%d hitboxes=%d anims=%d/%d seqs=%d/%d animblocks=%d/%d channels=%d\n",
           g_mdl.bone_count, g_mdl.source_numbones,
           g_mdl.texture_count, g_mdl.source_numtextures,
           g_mdl.bodypart_count, g_mdl.source_numbodyparts,
           g_mdl.model_count, g_mdl.mesh_count, g_mdl.hitbox_count,
           g_mdl.anim_count, g_mdl.source_numlocalanim,
           g_mdl.sequence_count, g_mdl.source_numlocalseq,
           g_mdl.animblock_count, g_mdl.source_numanimblocks,
           g_mdl.anim_channel_count);
    if (g_mdl.surfaceprop[0] != 0 || g_mdl.contents != 0) {
        printf("  surfaceprop=%s contents=%d mass=", g_mdl.surfaceprop, g_mdl.contents);
        print_fixed(g_mdl.mass);
        printf("\n");
    }
    for (i = 0; i < g_mdl.bone_count && i < 8; ++i) {
        printf("  bone[%d] parent=%d name=%s pos=", i, g_mdl.bones[i].parent, g_mdl.bones[i].name);
        print_fixed(g_mdl.bones[i].pos.x);
        printf(",");
        print_fixed(g_mdl.bones[i].pos.y);
        printf(",");
        print_fixed(g_mdl.bones[i].pos.z);
        printf("\n");
    }
    for (i = 0; i < g_mdl.texture_count && i < 8; ++i) {
        printf("  texture[%d] %s flags=%d\n", i, g_mdl.textures[i].name, g_mdl.textures[i].flags);
    }
    for (i = 0; i < g_mdl.anim_count && i < 4; ++i) {
        printf("  anim[%d] name=%s frames=%d block=%d index=%d channels=%d flags_or=%d fps=",
               i, g_mdl.anims[i].name, g_mdl.anims[i].numframes,
               g_mdl.anims[i].animblock, g_mdl.anims[i].animindex,
               g_mdl.anims[i].anim_channel_count, g_mdl.anims[i].anim_flags_or);
        print_fixed(g_mdl.anims[i].fps);
        printf("\n");
    }
    for (i = 0; i < g_mdl.sequence_count && i < 4; ++i) {
        printf("  seq[%d] label=%s activity=%s blends=%d events=%d\n",
               i, g_mdl.sequences[i].label, g_mdl.sequences[i].activity_name,
               g_mdl.sequences[i].numblends, g_mdl.sequences[i].numevents);
    }
}

static void print_vvd(void)
{
    printf("VVD version=%d checksum=%d lods=%d vertices=%d/%d fixups=%d/%d\n",
           g_vvd.version, g_vvd.checksum, g_vvd.num_lods,
           g_vvd.vertex_count, g_vvd.source_vertex_count,
           g_vvd.fixup_count, g_vvd.source_num_fixups);
    if (g_vvd.vertex_count > 0) {
        printf("  v[0] pos=");
        print_fixed(g_vvd.vertices[0].position.x);
        printf(",");
        print_fixed(g_vvd.vertices[0].position.y);
        printf(",");
        print_fixed(g_vvd.vertices[0].position.z);
        printf(" uv=");
        print_fixed(g_vvd.vertices[0].texcoord.x);
        printf(",");
        print_fixed(g_vvd.vertices[0].texcoord.y);
        printf(" bones=%d\n", g_vvd.vertices[0].numbones);
    }
}

static void print_vtx(void)
{
    int i;
    printf("VTX version=%d checksum=%d bodyparts=%d/%d models=%d lods=%d meshes=%d stripgroups=%d strips=%d vertices=%d indices=%d matrepl=%d/%d\n",
           g_vtx.version, g_vtx.checksum,
           g_vtx.bodypart_count, g_vtx.source_num_bodyparts,
           g_vtx.model_count, g_vtx.lod_count, g_vtx.mesh_count,
           g_vtx.stripgroup_count, g_vtx.strip_count,
           g_vtx.vertex_count, g_vtx.index_count,
           g_vtx.material_replacement_count, g_vtx.source_material_replacement_count);
    for (i = 0; i < g_vtx.material_replacement_count && i < 8; ++i) {
        printf("  matrepl[%d] lod=%d material=%d -> %s\n", i,
               g_vtx.material_replacements[i].lod,
               g_vtx.material_replacements[i].material_id,
               g_vtx.material_replacements[i].replacement);
    }
}

static void print_smd(void)
{
    printf("SMD version=%d nodes=%d poses=%d materials=%d triangles=%d vertices=%d\n",
           g_smd.version, g_smd.node_count, g_smd.pose_count,
           g_smd.material_count, g_smd.triangle_count, g_smd.vertex_count);
    if (g_smd.node_count > 0) {
        printf("  node[0] id=%d parent=%d name=%s\n", g_smd.nodes[0].id, g_smd.nodes[0].parent, g_smd.nodes[0].name);
    }
    if (g_smd.material_count > 0) {
        printf("  material[0] %s\n", g_smd.materials[0]);
    }
}

static void print_qc(void)
{
    int i;
    printf("QC modelname=%s commands=%d bodies=%d sequences=%d cdmaterials=%d lods=%d attachments=%d hboxes=%d bones=%d poseparams=%d matrepl=%d collisions=%d flexctrl=%d flexrules=%d events=%d ikchains=%d ikrules=%d\n",
           g_qc.modelname, g_qc.command_count, g_qc.body_count,
           g_qc.sequence_count, g_qc.material_dir_count, g_qc.lod_count,
           g_qc.attachment_count, g_qc.qc_hitbox_count, g_qc.definebone_count,
           g_qc.poseparameter_count, g_qc.material_replace_count, g_qc.collision_count,
           g_qc.flexcontroller_count, g_qc.flexrule_count, g_qc.sequence_event_count,
           g_qc.ikchain_count, g_qc.ikrule_count);
    for (i = 0; i < g_qc.command_count && i < 12; ++i) {
        printf("  line=%d depth=%d cmd=%s argc=%d\n",
               g_qc.commands[i].line, g_qc.commands[i].depth,
               g_qc.commands[i].name, g_qc.commands[i].argc);
    }
    for (i = 0; i < g_qc.material_replace_count && i < 4; ++i) {
        printf("  matrepl[%d] %s -> %s depth=%d\n", i,
               g_qc.material_replaces[i].from_material,
               g_qc.material_replaces[i].to_material,
               g_qc.material_replaces[i].lod_depth);
    }
    for (i = 0; i < g_qc.collision_count && i < 4; ++i) {
        printf("  collision[%d] joints=%d source=%s\n", i,
               g_qc.collisions[i].joints, g_qc.collisions[i].source);
    }
    for (i = 0; i < g_qc.flexcontroller_count && i < 4; ++i) {
        printf("  flexcontroller[%d] %s class=%s range=%d..%d\n", i,
               g_qc.flexcontrollers[i].name, g_qc.flexcontrollers[i].group,
               g_qc.flexcontrollers[i].min_value, g_qc.flexcontrollers[i].max_value);
    }
    for (i = 0; i < g_qc.sequence_event_count && i < 4; ++i) {
        printf("  event[%d] seq=%s id=%d frame=%d options=%s\n", i,
               g_qc.sequence_events[i].sequence_name, g_qc.sequence_events[i].event_id,
               g_qc.sequence_events[i].frame, g_qc.sequence_events[i].options);
    }
    for (i = 0; i < g_qc.ikrule_count && i < 4; ++i) {
        printf("  ikrule[%d] seq=%s chain=%s bone=%s type=%d\n", i,
               g_qc.sequences[g_qc.ikrules[i].sequence_index].name, g_qc.ikrules[i].chain,
               g_qc.ikrules[i].bone, g_qc.ikrules[i].type);
    }
}

static void print_vmt(void)
{
    int i;
    if (hl2m_vmt_to_material_desc(&g_vmt, &g_mat) == HL2M_OK) {
        printf("VMT shader=%s params=%d base=%s surface=%s flags=%d\n", g_vmt.shader, g_vmt.param_count, g_mat.base_texture, g_mat.surfaceprop, g_mat.flags);
    } else {
        printf("VMT shader=%s params=%d\n", g_vmt.shader, g_vmt.param_count);
    }
    for (i = 0; i < g_vmt.param_count && i < 12; ++i) {
        printf("  depth=%d %s = %s\n", g_vmt.params[i].depth, g_vmt.params[i].key, g_vmt.params[i].value);
    }
}

static void print_phy(void)
{
    int i;
    printf("PHY header=%d id=%d checksum=%d solids=%d/%d keybytes=%d\n",
           g_phy.header_size, g_phy.id, g_phy.checksum,
           g_phy.solid_count, g_phy.source_solid_count, g_phy.keyvalue_size);
    for (i = 0; i < g_phy.solid_count && i < 8; ++i) {
        printf("  solid[%d] off=%d size=%d legacy=%d vphysics=%d surface=%d version=%d modeltype=%d\n",
               i, g_phy.solids[i].offset, g_phy.solids[i].size,
               g_phy.solids[i].legacy, g_phy.solids[i].vphysics_id,
               g_phy.solids[i].surface_size, g_phy.solids[i].version,
               g_phy.solids[i].model_type);
    }
}

static void print_vta(void)
{
    int i;
    printf("VTA version=%d flexes=%d vertices=%d\n", g_vta.version, g_vta.flex_count, g_vta.vertex_count);
    for (i = 0; i < g_vta.flex_count && i < 8; ++i) {
        printf("  flex[%d] time=%d name=%s verts=%d first=%d\n",
               i, g_vta.flexes[i].time, g_vta.flexes[i].name,
               g_vta.flexes[i].num_vertices, g_vta.flexes[i].first_vertex);
    }
}

static void print_ani(void)
{
    int i;
    printf("ANI name=%s version=%d checksum=%d blocks=%d referenced_anims=%d scanned_channels=%d missing=%d\n",
           g_ani.name, g_ani.version, g_ani.checksum, g_ani.animblock_count,
           g_ani.referenced_anim_count, g_ani.scanned_channel_count, g_ani.missing_block_count);
    for (i = 0; i < g_ani.animblock_count && i < 8; ++i) {
        printf("  block[%d] %d..%d bytes=%d\n", i,
               g_ani.animblocks[i].datastart, g_ani.animblocks[i].dataend,
               g_ani.animblocks[i].data_size);
    }
}

int main(int argc, char **argv)
{
    unsigned int size;
    int r;
    int fmt;
    int fixed;
    if (argc < 2) {
        printf("usage: hl2m_probe file.mdl|file.vvd|file.dx90.vtx|file.smd|file.vmt|file.qc|file.phy|file.ani|file.vta\n");
        return 1;
    }
    size = load_file(argv[1]);
    if (size == 0u) {
        printf("could not read file, or file is empty\n");
        return 2;
    }
    fmt = hl2m_guess_format(g_file_buffer, size, argv[1]);
    if (fmt == HL2M_FORMAT_MDL || fmt == HL2M_FORMAT_VVD ||
        fmt == HL2M_FORMAT_VTX || fmt == HL2M_FORMAT_VTF ||
        fmt == HL2M_FORMAT_VMT) {
        r = hl2m_repair_missing_first_byte(g_file_buffer, &size,
                         PROBE_BUFFER_SIZE, fmt, &fixed);
        if (r != HL2M_OK) {
            printf("input header repair error=%d\n",r);
            return 4;
        }
        if (fixed) printf("recovered missing first byte\n");
    }
    if (has_suffix(argv[1], ".mdl")) {
        r = hl2m_decode_mdl(g_file_buffer, size, &g_mdl);
        printf("decode result=%d\n", r);
        print_mdl();
    } else if (has_suffix(argv[1], ".vvd")) {
        r = hl2m_decode_vvd(g_file_buffer, size, 0, &g_vvd);
        printf("decode result=%d\n", r);
        print_vvd();
    } else if (has_suffix(argv[1], ".vtx")) {
        r = hl2m_decode_vtx(g_file_buffer, size, &g_vtx);
        printf("decode result=%d\n", r);
        print_vtx();
    } else if (has_suffix(argv[1], ".smd")) {
        r = hl2m_decode_smd(g_file_buffer, size, &g_smd);
        printf("decode result=%d\n", r);
        print_smd();
    } else if (has_suffix(argv[1], ".vmt")) {
        r = hl2m_decode_vmt(g_file_buffer, size, &g_vmt);
        printf("decode result=%d\n", r);
        print_vmt();
    } else if (has_suffix(argv[1], ".qc")) {
        r = hl2m_decode_qc(g_file_buffer, size, &g_qc);
        printf("decode result=%d\n", r);
        print_qc();
    } else if (has_suffix(argv[1], ".phy")) {
        r = hl2m_decode_phy(g_file_buffer, size, &g_phy);
        printf("decode result=%d\n", r);
        print_phy();
    } else if (has_suffix(argv[1], ".ani")) {
        r = hl2m_decode_ani(g_file_buffer, size, 0, &g_ani);
        printf("decode result=%d\n", r);
        print_ani();
    } else if (has_suffix(argv[1], ".vta")) {
        r = hl2m_decode_vta(g_file_buffer, size, &g_vta);
        printf("decode result=%d\n", r);
        print_vta();
    } else {
        printf("unknown suffix\n");
        return 3;
    }
    return 0;
}
