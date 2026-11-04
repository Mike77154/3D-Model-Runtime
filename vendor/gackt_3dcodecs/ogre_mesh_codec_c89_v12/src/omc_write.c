#include "ogre_mesh_codec.h"
#include <stdio.h>
#include <string.h>

static const char *omc_operation_to_string(OMC_U8 op)
{
    switch (op) {
    case OMC_OP_POINT_LIST: return "point_list";
    case OMC_OP_LINE_LIST: return "line_list";
    case OMC_OP_LINE_STRIP: return "line_strip";
    case OMC_OP_TRIANGLE_STRIP: return "triangle_strip";
    case OMC_OP_TRIANGLE_FAN: return "triangle_fan";
    case OMC_OP_PATCH_1_CONTROL_POINT: return "patch_1_control_point";
    case OMC_OP_PATCH_2_CONTROL_POINT: return "patch_2_control_point";
    case OMC_OP_PATCH_3_CONTROL_POINT: return "patch_3_control_point";
    case OMC_OP_PATCH_4_CONTROL_POINT: return "patch_4_control_point";
    case OMC_OP_PATCH_5_CONTROL_POINT: return "patch_5_control_point";
    case OMC_OP_PATCH_6_CONTROL_POINT: return "patch_6_control_point";
    case OMC_OP_PATCH_7_CONTROL_POINT: return "patch_7_control_point";
    case OMC_OP_PATCH_8_CONTROL_POINT: return "patch_8_control_point";
    case OMC_OP_PATCH_9_CONTROL_POINT: return "patch_9_control_point";
    case OMC_OP_PATCH_10_CONTROL_POINT: return "patch_10_control_point";
    case OMC_OP_PATCH_11_CONTROL_POINT: return "patch_11_control_point";
    case OMC_OP_PATCH_12_CONTROL_POINT: return "patch_12_control_point";
    case OMC_OP_PATCH_13_CONTROL_POINT: return "patch_13_control_point";
    case OMC_OP_PATCH_14_CONTROL_POINT: return "patch_14_control_point";
    case OMC_OP_PATCH_15_CONTROL_POINT: return "patch_15_control_point";
    case OMC_OP_PATCH_16_CONTROL_POINT: return "patch_16_control_point";
    case OMC_OP_PATCH_17_CONTROL_POINT: return "patch_17_control_point";
    case OMC_OP_PATCH_18_CONTROL_POINT: return "patch_18_control_point";
    case OMC_OP_PATCH_19_CONTROL_POINT: return "patch_19_control_point";
    case OMC_OP_PATCH_20_CONTROL_POINT: return "patch_20_control_point";
    case OMC_OP_PATCH_21_CONTROL_POINT: return "patch_21_control_point";
    case OMC_OP_PATCH_22_CONTROL_POINT: return "patch_22_control_point";
    case OMC_OP_PATCH_23_CONTROL_POINT: return "patch_23_control_point";
    case OMC_OP_PATCH_24_CONTROL_POINT: return "patch_24_control_point";
    case OMC_OP_PATCH_25_CONTROL_POINT: return "patch_25_control_point";
    case OMC_OP_PATCH_26_CONTROL_POINT: return "patch_26_control_point";
    case OMC_OP_PATCH_27_CONTROL_POINT: return "patch_27_control_point";
    case OMC_OP_PATCH_28_CONTROL_POINT: return "patch_28_control_point";
    case OMC_OP_PATCH_29_CONTROL_POINT: return "patch_29_control_point";
    case OMC_OP_PATCH_30_CONTROL_POINT: return "patch_30_control_point";
    case OMC_OP_PATCH_31_CONTROL_POINT: return "patch_31_control_point";
    case OMC_OP_PATCH_32_CONTROL_POINT: return "patch_32_control_point";
    case OMC_OP_LINE_LIST_ADJ: return "line_list_adj";
    case OMC_OP_LINE_STRIP_ADJ: return "line_strip_adj";
    case OMC_OP_TRIANGLE_LIST_ADJ: return "triangle_list_adj";
    case OMC_OP_TRIANGLE_STRIP_ADJ: return "triangle_strip_adj";
    case OMC_OP_TRIANGLE_LIST:
    default: return "triangle_list";
    }
}

static void omc_fprint_fixed(FILE *f, OMC_Fixed v)
{
    char b[48];
    omc_fixed_to_string(v, b, sizeof(b), 5);
    fputs(b, f);
}

static void omc_fprint_xml_attr(FILE *f, const char *s)
{
    unsigned long i;
    unsigned char c;

    if (!f || !s) return;
    i = 0UL;
    while (s[i]) {
        c = (unsigned char)s[i];
        if (c == (unsigned char)'&') fputs("&amp;", f);
        else if (c == (unsigned char)'<') fputs("&lt;", f);
        else if (c == (unsigned char)'>') fputs("&gt;", f);
        else if (c == (unsigned char)'\"') fputs("&quot;", f);
        else if (c == (unsigned char)'\'') fputs("&apos;", f);
        else if (c < 32U && c != 9U && c != 10U && c != 13U) fputc('_', f);
        else fputc((int)c, f);
        i++;
    }
}

static void omc_fprint_colour_rgba(FILE *f, OMC_U32 rgba)
{
    OMC_Fixed c;
    OMC_U32 r;
    OMC_U32 g;
    OMC_U32 b;
    OMC_U32 a;

    r = (rgba >> 24) & 255UL;
    g = (rgba >> 16) & 255UL;
    b = (rgba >> 8) & 255UL;
    a = rgba & 255UL;

    c = (OMC_Fixed)((long)r * OMC_FIXED_ONE / 255L);
    omc_fprint_fixed(f, c);
    fprintf(f, " ");
    c = (OMC_Fixed)((long)g * OMC_FIXED_ONE / 255L);
    omc_fprint_fixed(f, c);
    fprintf(f, " ");
    c = (OMC_Fixed)((long)b * OMC_FIXED_ONE / 255L);
    omc_fprint_fixed(f, c);
    fprintf(f, " ");
    c = (OMC_Fixed)((long)a * OMC_FIXED_ONE / 255L);
    omc_fprint_fixed(f, c);
}

static void omc_write_vertex(FILE *f, const OMC_Vertex *v)
{
    unsigned long i;
    fprintf(f, "      <vertex>\n");
    if ((v->flags & OMC_VTX_HAS_POSITION) != 0U) {
        fprintf(f, "        <position x=\""); omc_fprint_fixed(f, v->position.x);
        fprintf(f, "\" y=\""); omc_fprint_fixed(f, v->position.y);
        fprintf(f, "\" z=\""); omc_fprint_fixed(f, v->position.z);
        fprintf(f, "\" />\n");
    }
    if ((v->flags & OMC_VTX_HAS_NORMAL) != 0U) {
        fprintf(f, "        <normal x=\""); omc_fprint_fixed(f, v->normal.x);
        fprintf(f, "\" y=\""); omc_fprint_fixed(f, v->normal.y);
        fprintf(f, "\" z=\""); omc_fprint_fixed(f, v->normal.z);
        fprintf(f, "\" />\n");
    }
    if ((v->flags & OMC_VTX_HAS_TANGENT) != 0U) {
        fprintf(f, "        <tangent x=\""); omc_fprint_fixed(f, v->tangent.x);
        fprintf(f, "\" y=\""); omc_fprint_fixed(f, v->tangent.y);
        fprintf(f, "\" z=\""); omc_fprint_fixed(f, v->tangent.z);
        fprintf(f, "\" w=\""); omc_fprint_fixed(f, v->tangent.w);
        fprintf(f, "\" />\n");
    }
    if ((v->flags & OMC_VTX_HAS_BINORMAL) != 0U) {
        fprintf(f, "        <binormal x=\""); omc_fprint_fixed(f, v->binormal.x);
        fprintf(f, "\" y=\""); omc_fprint_fixed(f, v->binormal.y);
        fprintf(f, "\" z=\""); omc_fprint_fixed(f, v->binormal.z);
        fprintf(f, "\" />\n");
    }
    if ((v->flags & OMC_VTX_HAS_DIFFUSE) != 0U) {
        fprintf(f, "        <colour_diffuse value=\"");
        omc_fprint_colour_rgba(f, v->diffuse_rgba);
        fprintf(f, "\" />\n");
    }
    if ((v->flags & OMC_VTX_HAS_SPECULAR) != 0U) {
        fprintf(f, "        <colour_specular value=\"");
        omc_fprint_colour_rgba(f, v->specular_rgba);
        fprintf(f, "\" />\n");
    }
    i = 0UL;
    while (i < (unsigned long)v->texcoord_count && i < (unsigned long)OMC_TEXCOORD_SETS) {
        fprintf(f, "        <texcoord u=\""); omc_fprint_fixed(f, v->texcoord[i].u);
        fprintf(f, "\" v=\""); omc_fprint_fixed(f, v->texcoord[i].v);
        if (v->texcoord_dims[i] >= 3U) {
            fprintf(f, "\" w=\""); omc_fprint_fixed(f, v->texcoord_w[i]);
        }
        if (v->texcoord_dims[i] >= 4U) {
            fprintf(f, "\" x=\""); omc_fprint_fixed(f, v->texcoord_x[i]);
        }
        fprintf(f, "\" />\n");
        i++;
    }
    fprintf(f, "      </vertex>\n");
}

static void omc_write_vertexbuffer(FILE *f, const OMC_Mesh *mesh, OMC_U32 start, OMC_U32 count)
{
    OMC_U32 i;
    int has_pos;
    int has_norm;
    int has_tan;
    int has_bin;
    int has_diff;
    int has_spec;
    unsigned long max_uv;
    OMC_U8 uv_dims[OMC_TEXCOORD_SETS];
    const OMC_Vertex *v;

    has_pos = 0;
    has_norm = 0;
    has_tan = 0;
    has_bin = 0;
    has_diff = 0;
    has_spec = 0;
    max_uv = 0UL;
    i = 0UL;
    while (i < (OMC_U32)OMC_TEXCOORD_SETS) {
        uv_dims[i] = 2U;
        i++;
    }
    i = 0UL;
    while (i < count && start + i < mesh->vertex_count) {
        v = &mesh->vertices[start + i];
        if ((v->flags & OMC_VTX_HAS_POSITION) != 0U) has_pos = 1;
        if ((v->flags & OMC_VTX_HAS_NORMAL) != 0U) has_norm = 1;
        if ((v->flags & OMC_VTX_HAS_TANGENT) != 0U) has_tan = 1;
        if ((v->flags & OMC_VTX_HAS_BINORMAL) != 0U) has_bin = 1;
        if ((v->flags & OMC_VTX_HAS_DIFFUSE) != 0U) has_diff = 1;
        if ((v->flags & OMC_VTX_HAS_SPECULAR) != 0U) has_spec = 1;
        if ((unsigned long)v->texcoord_count > max_uv) max_uv = (unsigned long)v->texcoord_count;
        if (v->texcoord_count > 0U) {
            unsigned long uvi;
            uvi = 0UL;
            while (uvi < (unsigned long)v->texcoord_count && uvi < (unsigned long)OMC_TEXCOORD_SETS) {
                if (v->texcoord_dims[uvi] > uv_dims[uvi]) uv_dims[uvi] = v->texcoord_dims[uvi];
                uvi++;
            }
        }
        i++;
    }

    fprintf(f, "    <vertexbuffer positions=\"%s\" normals=\"%s\" tangents=\"%s\" binormals=\"%s\" colours_diffuse=\"%s\" colours_specular=\"%s\" texture_coords=\"%lu\"",
            has_pos ? "true" : "false",
            has_norm ? "true" : "false",
            has_tan ? "true" : "false",
            has_bin ? "true" : "false",
            has_diff ? "true" : "false",
            has_spec ? "true" : "false",
            max_uv);
    i = 0UL;
    while (i < max_uv && i < (unsigned long)OMC_TEXCOORD_SETS) {
        fprintf(f, " texture_coord_dimensions_%lu=\"%u\"", i, (unsigned int)(uv_dims[i] == 0U ? 2U : uv_dims[i]));
        i++;
    }
    if (has_tan) fprintf(f, " tangent_dimensions=\"4\"");
    fprintf(f, ">\n");
    i = 0UL;
    while (i < count && start + i < mesh->vertex_count) {
        omc_write_vertex(f, &mesh->vertices[start + i]);
        i++;
    }
    fprintf(f, "    </vertexbuffer>\n");
}

static void omc_write_geometry(FILE *f, const OMC_Mesh *mesh, OMC_U32 start, OMC_U32 count, const char *tag)
{
    fprintf(f, "  <%s vertexcount=\"%lu\">\n", tag, (unsigned long)count);
    omc_write_vertexbuffer(f, mesh, start, count);
    fprintf(f, "  </%s>\n", tag);
}

static void omc_write_submesh_geometry(FILE *f, const OMC_Mesh *mesh, const OMC_Submesh *sm)
{
    fprintf(f, "    <geometry vertexcount=\"%lu\">\n", (unsigned long)sm->vertex_count);
    omc_write_vertexbuffer(f, mesh, sm->vertex_start, sm->vertex_count);
    fprintf(f, "    </geometry>\n");
}

static void omc_write_mesh_bones(FILE *f, const OMC_Mesh *mesh)
{
    OMC_U32 i;
    if (mesh->bone_assignment_count == 0UL) return;
    fprintf(f, "  <boneassignments>\n");
    i = 0UL;
    while (i < mesh->bone_assignment_count) {
        fprintf(f, "    <vertexboneassignment vertexindex=\"%lu\" boneindex=\"%u\" weight=\"",
                (unsigned long)mesh->bone_assignments[i].vertex_index,
                (unsigned int)mesh->bone_assignments[i].bone_index);
        omc_fprint_fixed(f, mesh->bone_assignments[i].weight);
        fprintf(f, "\" />\n");
        i++;
    }
    fprintf(f, "  </boneassignments>\n");
}


static void omc_write_texture_aliases(FILE *f, const OMC_Mesh *mesh, OMC_U16 submesh_index)
{
    OMC_U32 i;
    int opened;
    opened = 0;
    i = 0UL;
    while (i < mesh->texture_alias_count) {
        if (mesh->texture_aliases[i].submesh_index == submesh_index) {
            if (!opened) {
                fprintf(f, "      <textures>\n");
                opened = 1;
            }
            fprintf(f, "        <texture alias=\"");
            omc_fprint_xml_attr(f, mesh->texture_aliases[i].alias);
            fprintf(f, "\" name=\"");
            omc_fprint_xml_attr(f, mesh->texture_aliases[i].texture);
            fprintf(f, "\" />\n");
        }
        i++;
    }
    if (opened) fprintf(f, "      </textures>\n");
}

static void omc_write_submesh_bones(FILE *f, const OMC_Mesh *mesh, OMC_U16 submesh_index)
{
    OMC_U32 i;
    int opened;
    opened = 0;
    i = 0UL;
    while (i < mesh->submesh_bone_assignment_count) {
        if (mesh->submesh_bone_assignments[i].submesh_index == submesh_index) {
            if (!opened) {
                fprintf(f, "    <boneassignments>\n");
                opened = 1;
            }
            fprintf(f, "      <vertexboneassignment vertexindex=\"%lu\" boneindex=\"%u\" weight=\"",
                    (unsigned long)mesh->submesh_bone_assignments[i].assignment.vertex_index,
                    (unsigned int)mesh->submesh_bone_assignments[i].assignment.bone_index);
            omc_fprint_fixed(f, mesh->submesh_bone_assignments[i].assignment.weight);
            fprintf(f, "\" />\n");
        }
        i++;
    }
    if (opened) fprintf(f, "    </boneassignments>\n");
}

static void omc_write_poses(FILE *f, const OMC_Mesh *mesh)
{
    OMC_U16 i;
    OMC_U32 j;
    OMC_Pose *p;
    OMC_PoseOffset *o;
    if (mesh->pose_count == 0U) return;
    fprintf(f, "  <poses>\n");
    i = 0U;
    while (i < mesh->pose_count) {
        p = (OMC_Pose *)&mesh->poses[i];
        fprintf(f, "    <pose target=\"%s\" index=\"%u\" name=\"",
                p->target_is_submesh ? "submesh" : "mesh",
                (unsigned int)p->target_index);
        omc_fprint_xml_attr(f, p->name);
        fprintf(f, "\">\n");
        j = p->offset_start;
        while (j < p->offset_start + p->offset_count && j < mesh->pose_offset_count) {
            o = (OMC_PoseOffset *)&mesh->pose_offsets[j];
            fprintf(f, "      <poseoffset index=\"%lu\" x=\"", (unsigned long)o->vertex_index);
            omc_fprint_fixed(f, o->offset.x);
            fprintf(f, "\" y=\""); omc_fprint_fixed(f, o->offset.y);
            fprintf(f, "\" z=\""); omc_fprint_fixed(f, o->offset.z);
            if (o->has_normal) {
                fprintf(f, "\" nx=\""); omc_fprint_fixed(f, o->normal.x);
                fprintf(f, "\" ny=\""); omc_fprint_fixed(f, o->normal.y);
                fprintf(f, "\" nz=\""); omc_fprint_fixed(f, o->normal.z);
            }
            fprintf(f, "\" />\n");
            j++;
        }
        fprintf(f, "    </pose>\n");
        i++;
    }
    fprintf(f, "  </poses>\n");
}

static void omc_write_morph_keyframe(FILE *f, const OMC_Mesh *mesh, OMC_U32 morph_key_index)
{
    const OMC_MorphKey *mk;
    const OMC_MorphVertex *mv;
    OMC_U32 vi;

    if (morph_key_index >= mesh->morph_key_count) return;
    mk = &mesh->morph_keys[morph_key_index];
    fprintf(f, "            <keyframe time=\"");
    omc_fprint_fixed(f, mk->time);
    fprintf(f, "\">\n");
    vi = mk->vertex_start;
    while (vi < mk->vertex_start + mk->vertex_count && vi < mesh->morph_vertex_count) {
        mv = &mesh->morph_vertices[vi];
        fprintf(f, "              <position x=\""); omc_fprint_fixed(f, mv->position.x);
        fprintf(f, "\" y=\""); omc_fprint_fixed(f, mv->position.y);
        fprintf(f, "\" z=\""); omc_fprint_fixed(f, mv->position.z);
        fprintf(f, "\" />\n");
        if (mv->has_normal) {
            fprintf(f, "              <normal x=\""); omc_fprint_fixed(f, mv->normal.x);
            fprintf(f, "\" y=\""); omc_fprint_fixed(f, mv->normal.y);
            fprintf(f, "\" z=\""); omc_fprint_fixed(f, mv->normal.z);
            fprintf(f, "\" />\n");
        }
        vi++;
    }
    fprintf(f, "            </keyframe>\n");
}

static void omc_write_animations(FILE *f, const OMC_Mesh *mesh)
{
    OMC_U16 ai;
    OMC_U32 ti;
    OMC_U32 ki;
    OMC_U32 mi;
    OMC_Fixed current_time;
    int open_keyframe;
    OMC_Animation *a;
    OMC_AnimationTrack *t;
    OMC_AnimationKey *k;
    if (mesh->animation_count == 0U) return;
    fprintf(f, "  <animations>\n");
    ai = 0U;
    while (ai < mesh->animation_count) {
        a = (OMC_Animation *)&mesh->animations[ai];
        fprintf(f, "    <animation name=\"");
        omc_fprint_xml_attr(f, a->name);
        fprintf(f, "\" length=\"");
        omc_fprint_fixed(f, a->length);
        fprintf(f, "\">\n");
        if (a->has_baseinfo) {
            fprintf(f, "      <baseinfo baseanimationname=\"");
            omc_fprint_xml_attr(f, a->base_animation_name);
            fprintf(f, "\" basekeyframetime=\"");
            omc_fprint_fixed(f, a->base_keyframe_time);
            fprintf(f, "\" />\n");
        }
        fprintf(f, "      <tracks>\n");
        ti = a->track_start;
        while (ti < a->track_start + a->track_count && ti < mesh->track_count) {
            t = (OMC_AnimationTrack *)&mesh->tracks[ti];
            fprintf(f, "        <track target=\"%s\" index=\"%u\" type=\"%s\">\n          <keyframes>\n",
                    t->target_is_submesh ? "submesh" : "mesh",
                    (unsigned int)t->target_index,
                    t->is_pose_track ? "pose" : "morph");
            if (t->is_pose_track) {
                ki = t->key_start;
                while (ki < t->key_start + t->key_count && ki < mesh->key_count) {
                    k = (OMC_AnimationKey *)&mesh->keys[ki];
                    current_time = k->time;
                    fprintf(f, "            <keyframe time=\"");
                    omc_fprint_fixed(f, current_time);
                    fprintf(f, "\">");
                    open_keyframe = 1;
                    while (ki < t->key_start + t->key_count && ki < mesh->key_count && mesh->keys[ki].time == current_time) {
                        k = (OMC_AnimationKey *)&mesh->keys[ki];
                        fprintf(f, "<poseref poseindex=\"%u\" influence=\"", (unsigned int)k->pose_index);
                        omc_fprint_fixed(f, k->influence);
                        fprintf(f, "\" />");
                        ki++;
                    }
                    if (open_keyframe) fprintf(f, "</keyframe>\n");
                }
            } else {
                mi = t->morph_key_start;
                while (mi < t->morph_key_start + t->morph_key_count && mi < mesh->morph_key_count) {
                    omc_write_morph_keyframe(f, mesh, mi);
                    mi++;
                }
            }
            fprintf(f, "          </keyframes>\n        </track>\n");
            ti++;
        }
        fprintf(f, "      </tracks>\n    </animation>\n");
        ai++;
    }
    fprintf(f, "  </animations>\n");
}


static void omc_write_lods(FILE *f, const OMC_Mesh *mesh)
{
    OMC_U16 li;
    OMC_U32 fi;
    OMC_U32 end;
    OMC_U16 current_submesh;
    int open_list;
    const OMC_LodLevel *lod;
    const OMC_LodFace *face;

    if (mesh->lod_level_count == 0U) return;
    fprintf(f, "  <levelofdetail strategy=\"");
    omc_fprint_xml_attr(f, mesh->lod_strategy[0] ? mesh->lod_strategy : "Distance");
    fprintf(f, "\" numlevels=\"%u\" manual=\"%s\">\n",
            (unsigned int)(mesh->lod_declared_levels ? mesh->lod_declared_levels : mesh->lod_level_count),
            mesh->lod_manual ? "true" : "false");
    li = 0U;
    while (li < mesh->lod_level_count) {
        lod = &mesh->lod_levels[li];
        if (lod->manual) {
            fprintf(f, "    <lodmanual value=\"");
            omc_fprint_fixed(f, lod->value);
            fprintf(f, "\" meshname=\"");
            omc_fprint_xml_attr(f, lod->mesh_name);
            fprintf(f, "\" />\n");
        } else {
            fprintf(f, "    <lodgenerated value=\"");
            omc_fprint_fixed(f, lod->value);
            fprintf(f, "\" meshname=\"");
            omc_fprint_xml_attr(f, lod->mesh_name);
            fprintf(f, "\">\n");
            fi = lod->face_start;
            end = lod->face_start + lod->face_count;
            while (fi < end && fi < mesh->lod_face_count) {
                current_submesh = mesh->lod_faces[fi].submesh_index;
                fprintf(f, "      <lodfacelist submeshindex=\"%u\" numfaces=\"", (unsigned int)current_submesh);
                {
                    OMC_U32 cj;
                    OMC_U32 cnt;
                    cj = fi;
                    cnt = 0UL;
                    while (cj < end && cj < mesh->lod_face_count && mesh->lod_faces[cj].submesh_index == current_submesh) { cnt++; cj++; }
                    fprintf(f, "%lu\">\n", (unsigned long)cnt);
                }
                open_list = 1;
                while (fi < end && fi < mesh->lod_face_count && mesh->lod_faces[fi].submesh_index == current_submesh) {
                    face = &mesh->lod_faces[fi];
                    fprintf(f, "        <face v1=\"%lu\" v2=\"%lu\" v3=\"%lu\" />\n",
                            (unsigned long)face->v1, (unsigned long)face->v2, (unsigned long)face->v3);
                    fi++;
                }
                if (open_list) fprintf(f, "      </lodfacelist>\n");
            }
            fprintf(f, "    </lodgenerated>\n");
        }
        li++;
    }
    fprintf(f, "  </levelofdetail>\n");
}

static void omc_write_extremes(FILE *f, const OMC_Mesh *mesh)
{
    OMC_U32 i;
    OMC_U16 s;
    int any;

    if (mesh->extreme_count == 0UL) return;
    fprintf(f, "  <extremes>\n");
    s = 0U;
    while (s < mesh->submesh_count) {
        any = 0;
        i = 0UL;
        while (i < mesh->extreme_count) {
            if (mesh->extremes[i].submesh_index == s) any = 1;
            i++;
        }
        if (any) {
            fprintf(f, "    <submesh_extremes index=\"%u\">\n", (unsigned int)s);
            i = 0UL;
            while (i < mesh->extreme_count) {
                if (mesh->extremes[i].submesh_index == s) {
                    fprintf(f, "      <position x=\""); omc_fprint_fixed(f, mesh->extremes[i].position.x);
                    fprintf(f, "\" y=\""); omc_fprint_fixed(f, mesh->extremes[i].position.y);
                    fprintf(f, "\" z=\""); omc_fprint_fixed(f, mesh->extremes[i].position.z);
                    fprintf(f, "\" />\n");
                }
                i++;
            }
            fprintf(f, "    </submesh_extremes>\n");
        }
        s++;
    }
    fprintf(f, "  </extremes>\n");
}

int omc_save_mesh_xml_file(const OMC_Mesh *mesh, const char *path)
{
    FILE *f;
    OMC_U16 si;
    OMC_U32 j;
    const OMC_Submesh *sm;

    if (!mesh || !path) return OMC_ERR_NULL;
    f = fopen(path, "wb");
    if (!f) return OMC_ERR_IO;

    fprintf(f, "<mesh>\n");
    if (mesh->shared_vertex_count > 0UL) {
        omc_write_geometry(f, mesh, 0UL, mesh->shared_vertex_count, "sharedgeometry");
    }

    fprintf(f, "  <submeshes>\n");
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        fprintf(f, "    <submesh material=\"");
        omc_fprint_xml_attr(f, sm->material[0] ? sm->material : "BaseWhite");
        fprintf(f, "\" usesharedvertices=\"%s\" use32bitindexes=\"%s\" operationtype=\"%s\">\n",
                sm->use_shared_vertices ? "true" : "false",
                sm->use_32bit_indices ? "true" : "false",
                omc_operation_to_string(sm->operation_type));
        omc_write_texture_aliases(f, mesh, si);
        fprintf(f, "      <faces count=\"%lu\">\n", (unsigned long)(sm->index_count / 3UL));
        j = sm->index_start;
        while (j + 2UL < sm->index_start + sm->index_count && j + 2UL < mesh->index_count) {
            fprintf(f, "        <face v1=\"%lu\" v2=\"%lu\" v3=\"%lu\" />\n",
                    (unsigned long)mesh->indices[j],
                    (unsigned long)mesh->indices[j + 1UL],
                    (unsigned long)mesh->indices[j + 2UL]);
            j += 3UL;
        }
        fprintf(f, "      </faces>\n");
        if (!sm->use_shared_vertices || sm->has_dedicated_geometry) {
            omc_write_submesh_geometry(f, mesh, sm);
        }
        omc_write_submesh_bones(f, mesh, si);
        fprintf(f, "    </submesh>\n");
        si++;
    }
    fprintf(f, "  </submeshes>\n");

    if (mesh->skeleton_name[0]) {
        fprintf(f, "  <skeletonlink name=\"");
        omc_fprint_xml_attr(f, mesh->skeleton_name);
        fprintf(f, "\" />\n");
    }
    omc_write_mesh_bones(f, mesh);
    omc_write_lods(f, mesh);

    if (mesh->submesh_count > 0U) {
        int any_names;
        any_names = 0;
        si = 0U;
        while (si < mesh->submesh_count) {
            if (mesh->submeshes[si].name[0]) any_names = 1;
            si++;
        }
        if (any_names) {
            fprintf(f, "  <submeshnames>\n");
            si = 0U;
            while (si < mesh->submesh_count) {
                if (mesh->submeshes[si].name[0]) {
                    fprintf(f, "    <submeshname name=\"");
                    omc_fprint_xml_attr(f, mesh->submeshes[si].name);
                    fprintf(f, "\" index=\"%u\" />\n", (unsigned int)si);
                }
                si++;
            }
            fprintf(f, "  </submeshnames>\n");
        }
    }

    omc_write_poses(f, mesh);
    omc_write_animations(f, mesh);
    omc_write_extremes(f, mesh);
    fprintf(f, "</mesh>\n");
    fclose(f);
    return OMC_OK;
}

#define OMC_BW_CHUNK_HEADER 0x1000u
#define OMC_BW_CHUNK_MESH 0x3000u
#define OMC_BW_CHUNK_SUBMESH 0x4000u
#define OMC_BW_CHUNK_SUBMESH_OPERATION 0x4010u
#define OMC_BW_CHUNK_SUBMESH_BONE_ASSIGNMENT 0x4100u
#define OMC_BW_CHUNK_SUBMESH_TEXTURE_ALIAS 0x4200u
#define OMC_BW_CHUNK_GEOMETRY 0x5000u
#define OMC_BW_CHUNK_GEOMETRY_DECL 0x5100u
#define OMC_BW_CHUNK_GEOMETRY_ELEMENT 0x5110u
#define OMC_BW_CHUNK_GEOMETRY_VBUF 0x5200u
#define OMC_BW_CHUNK_GEOMETRY_VBUF_DATA 0x5210u
#define OMC_BW_CHUNK_SKELETON_LINK 0x6000u
#define OMC_BW_CHUNK_MESH_BONE_ASSIGNMENT 0x7000u
#define OMC_BW_CHUNK_LOD_LEVEL 0x8000u
#define OMC_BW_CHUNK_LOD_MANUAL 0x8110u
#define OMC_BW_CHUNK_LOD_GENERATED 0x8120u
#define OMC_BW_CHUNK_MESH_BOUNDS 0x9000u
#define OMC_BW_CHUNK_SUBMESH_NAME_TABLE 0xA000u
#define OMC_BW_CHUNK_SUBMESH_NAME_TABLE_ELEMENT 0xA100u
#define OMC_BW_CHUNK_EDGE_LISTS 0xB000u
#define OMC_BW_CHUNK_EDGE_LIST_LOD 0xB100u
#define OMC_BW_CHUNK_EDGE_GROUP 0xB110u
#define OMC_BW_CHUNK_POSES 0xC000u
#define OMC_BW_CHUNK_POSE 0xC100u
#define OMC_BW_CHUNK_POSE_VERTEX 0xC111u
#define OMC_BW_CHUNK_ANIMATIONS 0xD000u
#define OMC_BW_CHUNK_ANIMATION 0xD100u
#define OMC_BW_CHUNK_ANIMATION_BASEINFO 0xD105u
#define OMC_BW_CHUNK_ANIMATION_TRACK 0xD110u
#define OMC_BW_CHUNK_ANIMATION_MORPH_KEYFRAME 0xD111u
#define OMC_BW_CHUNK_ANIMATION_POSE_KEYFRAME 0xD112u
#define OMC_BW_CHUNK_ANIMATION_POSE_REF 0xD113u
#define OMC_BW_CHUNK_TABLE_EXTREMES 0xE000u

typedef struct OMC_BinWriteElemTag {
    OMC_U16 type;
    OMC_U16 semantic;
    OMC_U16 offset;
    OMC_U16 index;
} OMC_BinWriteElem;

typedef struct OMC_BinWriteLayoutTag {
    OMC_BinWriteElem elems[OMC_MAX_VERTEX_ELEMENTS];
    OMC_U16 elem_count;
    OMC_U16 stride;
    OMC_U8 uv_count;
    OMC_U8 uv_dims[OMC_TEXCOORD_SETS];
} OMC_BinWriteLayout;

static int omc_bw_u8(FILE *f, OMC_U8 v)
{
    return fputc((int)v, f) == EOF ? OMC_ERR_IO : OMC_OK;
}

static int omc_bw_u16(FILE *f, OMC_U16 v)
{
    if (fputc((int)(v & 255U), f) == EOF) return OMC_ERR_IO;
    if (fputc((int)((v >> 8) & 255U), f) == EOF) return OMC_ERR_IO;
    return OMC_OK;
}

static int omc_bw_u32(FILE *f, OMC_U32 v)
{
    if (fputc((int)(v & 255UL), f) == EOF) return OMC_ERR_IO;
    if (fputc((int)((v >> 8) & 255UL), f) == EOF) return OMC_ERR_IO;
    if (fputc((int)((v >> 16) & 255UL), f) == EOF) return OMC_ERR_IO;
    if (fputc((int)((v >> 24) & 255UL), f) == EOF) return OMC_ERR_IO;
    return OMC_OK;
}

static OMC_U32 omc_bw_fixed_to_ieee754(OMC_Fixed v)
{
    unsigned long sign;
    unsigned long mag;
    int p;
    int shift;
    unsigned long exp;
    unsigned long mant_full;
    unsigned long mant;
    unsigned long round_bit;
    unsigned long rem_mask;

    if (v == 0) return 0UL;
    sign = 0UL;
    if (v < 0) {
        sign = 1UL;
        mag = (unsigned long)(-v);
    } else {
        mag = (unsigned long)v;
    }
    if (mag == 0UL) return 0UL;

    p = 31;
    while (p > 0 && ((mag >> p) & 1UL) == 0UL) p--;
    exp = (unsigned long)(p + 111);

    if (p <= 23) {
        mant_full = mag << (23 - p);
        mant = mant_full & 0x7fffffUL;
    } else {
        shift = p - 23;
        mant_full = mag >> shift;
        mant = mant_full & 0x7fffffUL;
        round_bit = 1UL << (shift - 1);
        rem_mask = round_bit - 1UL;
        if ((mag & round_bit) != 0UL && (((mag & rem_mask) != 0UL) || ((mant & 1UL) != 0UL))) {
            mant++;
            if (mant >= 0x800000UL) {
                mant = 0UL;
                exp++;
            }
        }
    }

    if (exp >= 255UL) return sign ? 0xff800000UL : 0x7f800000UL;
    return ((sign & 1UL) << 31) | ((exp & 255UL) << 23) | (mant & 0x7fffffUL);
}

static int omc_bw_f32(FILE *f, OMC_Fixed v)
{
    return omc_bw_u32(f, omc_bw_fixed_to_ieee754(v));
}

static int omc_bw_string(FILE *f, const char *s)
{
    unsigned long i;
    i = 0UL;
    if (s) {
        while (s[i]) {
            if (fputc((int)(unsigned char)s[i], f) == EOF) return OMC_ERR_IO;
            i++;
        }
    }
    if (fputc('\n', f) == EOF) return OMC_ERR_IO;
    return OMC_OK;
}

static long omc_bw_begin_chunk(FILE *f, OMC_U16 id)
{
    long start;
    start = ftell(f);
    if (start < 0L) return -1L;
    if (omc_bw_u16(f, id) != OMC_OK) return -1L;
    if (omc_bw_u32(f, 0UL) != OMC_OK) return -1L;
    return start;
}

static int omc_bw_end_chunk(FILE *f, long start)
{
    long end;
    unsigned long size;
    if (start < 0L) return OMC_ERR_IO;
    end = ftell(f);
    if (end < start || end < 0L) return OMC_ERR_IO;
    size = (unsigned long)(end - start);
    if (fseek(f, start + 2L, SEEK_SET) != 0) return OMC_ERR_IO;
    if (omc_bw_u32(f, (OMC_U32)size) != OMC_OK) return OMC_ERR_IO;
    if (fseek(f, end, SEEK_SET) != 0) return OMC_ERR_IO;
    return OMC_OK;
}

static OMC_Fixed omc_bw_fixed_add_sat(OMC_Fixed a, OMC_Fixed b)
{
    if (b > 0 && a > (OMC_Fixed)2147483647L - b) return (OMC_Fixed)2147483647L;
    if (b < 0 && a < (OMC_Fixed)-2147483647L - b) return (OMC_Fixed)-2147483647L;
    return a + b;
}

static OMC_Fixed omc_bw_fixed_mul_q16(OMC_Fixed a, OMC_Fixed b)
{
    int neg;
    unsigned long au;
    unsigned long bu;
    unsigned long ah;
    unsigned long al;
    unsigned long bh;
    unsigned long bl;
    unsigned long high;
    unsigned long part;
    unsigned long r;
    unsigned long limit;

    limit = 2147483647UL;
    neg = 0;
    if (a < 0) {
        neg = !neg;
        if (a == (OMC_Fixed)(-2147483647L - 1L)) au = 2147483648UL;
        else au = (unsigned long)(-a);
    } else au = (unsigned long)a;
    if (b < 0) {
        neg = !neg;
        if (b == (OMC_Fixed)(-2147483647L - 1L)) bu = 2147483648UL;
        else bu = (unsigned long)(-b);
    } else bu = (unsigned long)b;
    if (au > limit) au = limit;
    if (bu > limit) bu = limit;

    ah = au >> 16;
    al = au & 65535UL;
    bh = bu >> 16;
    bl = bu & 65535UL;

    high = ah * bh;
    if (high > 32767UL) {
        r = limit;
    } else {
        r = high << 16;
        part = ah * bl;
        if (part > limit - r) r = limit;
        else r += part;
        part = al * bh;
        if (part > limit - r) r = limit;
        else r += part;
        part = (al * bl) >> 16;
        if (part > limit - r) r = limit;
        else r += part;
    }
    return neg ? -(OMC_Fixed)r : (OMC_Fixed)r;
}

static OMC_Fixed omc_bw_fixed_sqrt_q16(OMC_Fixed v)
{
    OMC_Fixed lo;
    OMC_Fixed hi;
    OMC_Fixed mid;
    OMC_Fixed sq;
    int i;

    if (v <= 0) return 0;
    lo = 0;
    hi = (v > OMC_FIXED_ONE) ? v : OMC_FIXED_ONE;
    if (hi > (OMC_Fixed)(32767L * OMC_FIXED_ONE)) hi = (OMC_Fixed)(32767L * OMC_FIXED_ONE);
    i = 0;
    while (i < 32) {
        mid = lo + ((hi - lo) / 2L);
        sq = omc_bw_fixed_mul_q16(mid, mid);
        if (sq <= v) lo = mid;
        else hi = mid;
        i++;
    }
    return lo;
}

static OMC_Fixed omc_bw_fixed_div_q16(OMC_Fixed a, OMC_Fixed b)
{
    int neg;
    unsigned long ua;
    unsigned long ub;
    unsigned long whole;
    unsigned long rem;
    unsigned long q;
    int bit;

    if (b == 0) return 0;
    neg = 0;
    if (a < 0) { neg = !neg; ua = (unsigned long)(-a); } else ua = (unsigned long)a;
    if (b < 0) { neg = !neg; ub = (unsigned long)(-b); } else ub = (unsigned long)b;
    if (ub == 0UL) return 0;
    whole = ua / ub;
    rem = ua % ub;
    if (whole > 32767UL) return neg ? (OMC_Fixed)-2147483647L : (OMC_Fixed)2147483647L;
    q = whole << OMC_FIXED_SHIFT;
    bit = OMC_FIXED_SHIFT - 1;
    while (bit >= 0) {
        rem <<= 1;
        if (rem >= ub) {
            rem -= ub;
            q |= (1UL << bit);
        }
        bit--;
    }
    if (q > 2147483647UL) q = 2147483647UL;
    return neg ? -(OMC_Fixed)q : (OMC_Fixed)q;
}

static OMC_Vec3 omc_bw_vec3_sub(OMC_Vec3 a, OMC_Vec3 b)
{
    OMC_Vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

static OMC_Vec3 omc_bw_vec3_cross(OMC_Vec3 a, OMC_Vec3 b)
{
    OMC_Vec3 r;
    r.x = omc_bw_fixed_mul_q16(a.y, b.z) - omc_bw_fixed_mul_q16(a.z, b.y);
    r.y = omc_bw_fixed_mul_q16(a.z, b.x) - omc_bw_fixed_mul_q16(a.x, b.z);
    r.z = omc_bw_fixed_mul_q16(a.x, b.y) - omc_bw_fixed_mul_q16(a.y, b.x);
    return r;
}

static OMC_Fixed omc_bw_vec3_dot(OMC_Vec3 a, OMC_Vec3 b)
{
    OMC_Fixed r;
    r = omc_bw_fixed_mul_q16(a.x, b.x);
    r = omc_bw_fixed_add_sat(r, omc_bw_fixed_mul_q16(a.y, b.y));
    r = omc_bw_fixed_add_sat(r, omc_bw_fixed_mul_q16(a.z, b.z));
    return r;
}

static OMC_Vec3 omc_bw_vec3_normalize(OMC_Vec3 v)
{
    OMC_Fixed len2;
    OMC_Fixed len;
    OMC_Vec3 r;
    len2 = omc_bw_fixed_add_sat(omc_bw_fixed_mul_q16(v.x, v.x), omc_bw_fixed_mul_q16(v.y, v.y));
    len2 = omc_bw_fixed_add_sat(len2, omc_bw_fixed_mul_q16(v.z, v.z));
    len = omc_bw_fixed_sqrt_q16(len2);
    if (len == 0) { r.x = 0; r.y = OMC_FIXED_ONE; r.z = 0; return r; }
    r.x = omc_bw_fixed_div_q16(v.x, len);
    r.y = omc_bw_fixed_div_q16(v.y, len);
    r.z = omc_bw_fixed_div_q16(v.z, len);
    return r;
}

static void omc_bw_get_bounds(const OMC_Mesh *mesh, OMC_Vec3 *mn, OMC_Vec3 *mx, OMC_Fixed *radius)
{
    OMC_U32 i;
    OMC_U8 first;
    OMC_Fixed dx;
    OMC_Fixed dy;
    OMC_Fixed dz;
    OMC_Fixed d2;
    OMC_Fixed max_d2;
    OMC_Vec3 center;
    const OMC_Vertex *v;

    if (mesh->has_bounds) {
        *mn = mesh->bounds_min;
        *mx = mesh->bounds_max;
        *radius = mesh->bounds_radius;
        return;
    }

    first = 1U;
    i = 0UL;
    while (i < mesh->vertex_count) {
        v = &mesh->vertices[i];
        if ((v->flags & OMC_VTX_HAS_POSITION) != 0U) {
            if (first) {
                *mn = v->position;
                *mx = v->position;
                first = 0U;
            } else {
                mn->x = omc_fixed_min(mn->x, v->position.x);
                mn->y = omc_fixed_min(mn->y, v->position.y);
                mn->z = omc_fixed_min(mn->z, v->position.z);
                mx->x = omc_fixed_max(mx->x, v->position.x);
                mx->y = omc_fixed_max(mx->y, v->position.y);
                mx->z = omc_fixed_max(mx->z, v->position.z);
            }
        }
        i++;
    }
    if (first) {
        mn->x = 0; mn->y = 0; mn->z = 0;
        mx->x = 0; mx->y = 0; mx->z = 0;
        *radius = 0;
        return;
    }
    center.x = (mn->x + mx->x) / 2L;
    center.y = (mn->y + mx->y) / 2L;
    center.z = (mn->z + mx->z) / 2L;
    max_d2 = 0;
    i = 0UL;
    while (i < mesh->vertex_count) {
        v = &mesh->vertices[i];
        if ((v->flags & OMC_VTX_HAS_POSITION) != 0U) {
            dx = v->position.x - center.x;
            dy = v->position.y - center.y;
            dz = v->position.z - center.z;
            d2 = omc_bw_fixed_add_sat(omc_bw_fixed_mul_q16(dx, dx), omc_bw_fixed_mul_q16(dy, dy));
            d2 = omc_bw_fixed_add_sat(d2, omc_bw_fixed_mul_q16(dz, dz));
            if (d2 > max_d2) max_d2 = d2;
        }
        i++;
    }
    *radius = omc_bw_fixed_sqrt_q16(max_d2);
}

static OMC_U32 omc_bw_rgba_to_abgr(OMC_U32 rgba)
{
    OMC_U32 r;
    OMC_U32 g;
    OMC_U32 b;
    OMC_U32 a;
    r = (rgba >> 24) & 255UL;
    g = (rgba >> 16) & 255UL;
    b = (rgba >> 8) & 255UL;
    a = rgba & 255UL;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

static OMC_U8 omc_bw_float_type_dims(OMC_U16 type)
{
    switch (type) {
    case OMC_VET_FLOAT1: return 1U;
    case OMC_VET_FLOAT2: return 2U;
    case OMC_VET_FLOAT3: return 3U;
    case OMC_VET_FLOAT4: return 4U;
    default: return 0U;
    }
}

static int omc_bw_elem_size(OMC_U16 type)
{
    switch (type) {
    case OMC_VET_FLOAT1: return 4;
    case OMC_VET_FLOAT2: return 8;
    case OMC_VET_FLOAT3: return 12;
    case OMC_VET_FLOAT4: return 16;
    case OMC_VET_COLOUR_ABGR: return 4;
    case OMC_VET_UBYTE4: return 4;
    default: return 0;
    }
}

static int omc_bw_layout_add(OMC_BinWriteLayout *l, OMC_U16 type, OMC_U16 semantic, OMC_U16 index)
{
    OMC_BinWriteElem *e;
    int size;
    size = omc_bw_elem_size(type);
    if (size <= 0) return OMC_ERR_FORMAT;
    if (l->elem_count >= (OMC_U16)OMC_MAX_VERTEX_ELEMENTS) return OMC_ERR_CAPACITY;
    if ((unsigned long)l->stride + (unsigned long)size > 65535UL) return OMC_ERR_CAPACITY;
    e = &l->elems[l->elem_count++];
    e->type = type;
    e->semantic = semantic;
    e->offset = l->stride;
    e->index = index;
    l->stride = (OMC_U16)(l->stride + (OMC_U16)size);
    return OMC_OK;
}

static int omc_bw_build_layout(const OMC_Mesh *mesh, OMC_U32 start, OMC_U32 count, OMC_BinWriteLayout *l)
{
    OMC_U32 i;
    OMC_U8 ui;
    OMC_U8 max_uv;
    OMC_U8 has_pos;
    OMC_U8 has_norm;
    OMC_U8 has_tan;
    OMC_U8 has_bin;
    OMC_U8 has_diff;
    OMC_U8 has_spec;
    OMC_U8 has_blend_weights;
    OMC_U8 has_blend_indices;
    const OMC_Vertex *v;
    int r;

    memset(l, 0, sizeof(*l));
    ui = 0U;
    while (ui < (OMC_U8)OMC_TEXCOORD_SETS) { l->uv_dims[ui] = 2U; ui++; }
    max_uv = 0U;
    has_pos = 0U; has_norm = 0U; has_tan = 0U; has_bin = 0U; has_diff = 0U; has_spec = 0U;
    has_blend_weights = 0U; has_blend_indices = 0U;
    i = 0UL;
    while (i < count && start + i < mesh->vertex_count) {
        v = &mesh->vertices[start + i];
        if ((v->flags & OMC_VTX_HAS_POSITION) != 0U) has_pos = 1U;
        if ((v->flags & OMC_VTX_HAS_BLEND_WEIGHTS) != 0U) has_blend_weights = 1U;
        if ((v->flags & OMC_VTX_HAS_BLEND_INDICES) != 0U) has_blend_indices = 1U;
        if ((v->flags & OMC_VTX_HAS_NORMAL) != 0U) has_norm = 1U;
        if ((v->flags & OMC_VTX_HAS_TANGENT) != 0U) has_tan = 1U;
        if ((v->flags & OMC_VTX_HAS_BINORMAL) != 0U) has_bin = 1U;
        if ((v->flags & OMC_VTX_HAS_DIFFUSE) != 0U) has_diff = 1U;
        if ((v->flags & OMC_VTX_HAS_SPECULAR) != 0U) has_spec = 1U;
        if (v->texcoord_count > max_uv) max_uv = v->texcoord_count;
        ui = 0U;
        while (ui < v->texcoord_count && ui < (OMC_U8)OMC_TEXCOORD_SETS) {
            if (v->texcoord_dims[ui] > l->uv_dims[ui]) l->uv_dims[ui] = v->texcoord_dims[ui];
            ui++;
        }
        i++;
    }
    if (max_uv > (OMC_U8)OMC_TEXCOORD_SETS) max_uv = (OMC_U8)OMC_TEXCOORD_SETS;
    l->uv_count = max_uv;

    if (has_pos) { r = omc_bw_layout_add(l, OMC_VET_FLOAT3, OMC_VES_POSITION, 0U); if (r != OMC_OK) return r; }
    if (has_blend_weights) { r = omc_bw_layout_add(l, OMC_VET_FLOAT4, OMC_VES_BLEND_WEIGHTS, 0U); if (r != OMC_OK) return r; }
    if (has_blend_indices) { r = omc_bw_layout_add(l, OMC_VET_UBYTE4, OMC_VES_BLEND_INDICES, 0U); if (r != OMC_OK) return r; }
    if (has_norm) { r = omc_bw_layout_add(l, OMC_VET_FLOAT3, OMC_VES_NORMAL, 0U); if (r != OMC_OK) return r; }
    if (has_tan) { r = omc_bw_layout_add(l, OMC_VET_FLOAT4, OMC_VES_TANGENT, 0U); if (r != OMC_OK) return r; }
    if (has_bin) { r = omc_bw_layout_add(l, OMC_VET_FLOAT3, OMC_VES_BINORMAL, 0U); if (r != OMC_OK) return r; }
    if (has_diff) { r = omc_bw_layout_add(l, OMC_VET_COLOUR_ABGR, OMC_VES_DIFFUSE, 0U); if (r != OMC_OK) return r; }
    if (has_spec) { r = omc_bw_layout_add(l, OMC_VET_COLOUR_ABGR, OMC_VES_SPECULAR, 0U); if (r != OMC_OK) return r; }
    ui = 0U;
    while (ui < max_uv) {
        OMC_U8 dims;
        dims = l->uv_dims[ui];
        if (dims < 1U || dims > 4U) dims = 2U;
        r = omc_bw_layout_add(l, (OMC_U16)(OMC_VET_FLOAT1 + (dims - 1U)), OMC_VES_TEXTURE_COORDINATES, ui);
        if (r != OMC_OK) return r;
        ui++;
    }
    return OMC_OK;
}

static int omc_bw_write_decl(FILE *f, const OMC_BinWriteLayout *l)
{
    long ch;
    long el;
    OMC_U16 i;
    int r;
    ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_GEOMETRY_DECL);
    if (ch < 0L) return OMC_ERR_IO;
    i = 0U;
    while (i < l->elem_count) {
        el = omc_bw_begin_chunk(f, OMC_BW_CHUNK_GEOMETRY_ELEMENT);
        if (el < 0L) return OMC_ERR_IO;
        r = omc_bw_u16(f, 0U); if (r != OMC_OK) return r;
        r = omc_bw_u16(f, l->elems[i].type); if (r != OMC_OK) return r;
        r = omc_bw_u16(f, l->elems[i].semantic); if (r != OMC_OK) return r;
        r = omc_bw_u16(f, l->elems[i].offset); if (r != OMC_OK) return r;
        r = omc_bw_u16(f, l->elems[i].index); if (r != OMC_OK) return r;
        r = omc_bw_end_chunk(f, el); if (r != OMC_OK) return r;
        i++;
    }
    return omc_bw_end_chunk(f, ch);
}

static int omc_bw_write_vertex_elem(FILE *f, const OMC_Vertex *v, const OMC_BinWriteElem *e)
{
    OMC_U8 dims;
    OMC_U8 idx;
    OMC_U32 raw;
    int r;

    switch (e->semantic) {
    case OMC_VES_POSITION:
        r = omc_bw_f32(f, v->position.x); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, v->position.y); if (r != OMC_OK) return r;
        return omc_bw_f32(f, v->position.z);
    case OMC_VES_NORMAL:
        r = omc_bw_f32(f, v->normal.x); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, v->normal.y); if (r != OMC_OK) return r;
        return omc_bw_f32(f, v->normal.z);
    case OMC_VES_BLEND_WEIGHTS:
        r = omc_bw_f32(f, v->blend_weights[0]); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, v->blend_weights[1]); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, v->blend_weights[2]); if (r != OMC_OK) return r;
        return omc_bw_f32(f, v->blend_weights[3]);
    case OMC_VES_BLEND_INDICES:
        r = omc_bw_u8(f, v->blend_indices[0] > 255U ? 255U : (OMC_U8)v->blend_indices[0]); if (r != OMC_OK) return r;
        r = omc_bw_u8(f, v->blend_indices[1] > 255U ? 255U : (OMC_U8)v->blend_indices[1]); if (r != OMC_OK) return r;
        r = omc_bw_u8(f, v->blend_indices[2] > 255U ? 255U : (OMC_U8)v->blend_indices[2]); if (r != OMC_OK) return r;
        return omc_bw_u8(f, v->blend_indices[3] > 255U ? 255U : (OMC_U8)v->blend_indices[3]);
    case OMC_VES_TANGENT:
        r = omc_bw_f32(f, v->tangent.x); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, v->tangent.y); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, v->tangent.z); if (r != OMC_OK) return r;
        return omc_bw_f32(f, v->tangent.w == 0 ? OMC_FIXED_ONE : v->tangent.w);
    case OMC_VES_BINORMAL:
        r = omc_bw_f32(f, v->binormal.x); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, v->binormal.y); if (r != OMC_OK) return r;
        return omc_bw_f32(f, v->binormal.z);
    case OMC_VES_DIFFUSE:
        raw = omc_bw_rgba_to_abgr(v->diffuse_rgba);
        return omc_bw_u32(f, raw);
    case OMC_VES_SPECULAR:
        raw = omc_bw_rgba_to_abgr(v->specular_rgba);
        return omc_bw_u32(f, raw);
    case OMC_VES_TEXTURE_COORDINATES:
        idx = (OMC_U8)e->index;
        dims = omc_bw_float_type_dims(e->type);
        if (idx >= (OMC_U8)OMC_TEXCOORD_SETS) return OMC_ERR_FORMAT;
        if (dims >= 1U) { r = omc_bw_f32(f, v->texcoord[idx].u); if (r != OMC_OK) return r; }
        if (dims >= 2U) { r = omc_bw_f32(f, v->texcoord[idx].v); if (r != OMC_OK) return r; }
        if (dims >= 3U) { r = omc_bw_f32(f, v->texcoord_w[idx]); if (r != OMC_OK) return r; }
        if (dims >= 4U) { r = omc_bw_f32(f, v->texcoord_x[idx]); if (r != OMC_OK) return r; }
        return OMC_OK;
    default:
        return OMC_ERR_FORMAT;
    }
}

static int omc_bw_write_vbuf(FILE *f, const OMC_Mesh *mesh, OMC_U32 start, OMC_U32 count, const OMC_BinWriteLayout *l)
{
    long vbuf;
    long data;
    OMC_U32 i;
    OMC_U16 e;
    const OMC_Vertex *v;
    int r;

    if (l->stride == 0U) return OMC_OK;
    vbuf = omc_bw_begin_chunk(f, OMC_BW_CHUNK_GEOMETRY_VBUF);
    if (vbuf < 0L) return OMC_ERR_IO;
    r = omc_bw_u16(f, 0U); if (r != OMC_OK) return r;
    r = omc_bw_u16(f, l->stride); if (r != OMC_OK) return r;
    data = omc_bw_begin_chunk(f, OMC_BW_CHUNK_GEOMETRY_VBUF_DATA);
    if (data < 0L) return OMC_ERR_IO;
    i = 0UL;
    while (i < count && start + i < mesh->vertex_count) {
        v = &mesh->vertices[start + i];
        e = 0U;
        while (e < l->elem_count) {
            r = omc_bw_write_vertex_elem(f, v, &l->elems[e]);
            if (r != OMC_OK) return r;
            e++;
        }
        i++;
    }
    r = omc_bw_end_chunk(f, data); if (r != OMC_OK) return r;
    return omc_bw_end_chunk(f, vbuf);
}

static int omc_bw_write_geometry(FILE *f, const OMC_Mesh *mesh, OMC_U32 start, OMC_U32 count)
{
    long geom;
    OMC_BinWriteLayout layout;
    int r;
    geom = omc_bw_begin_chunk(f, OMC_BW_CHUNK_GEOMETRY);
    if (geom < 0L) return OMC_ERR_IO;
    r = omc_bw_u32(f, count); if (r != OMC_OK) return r;
    r = omc_bw_build_layout(mesh, start, count, &layout); if (r != OMC_OK) return r;
    r = omc_bw_write_decl(f, &layout); if (r != OMC_OK) return r;
    r = omc_bw_write_vbuf(f, mesh, start, count, &layout); if (r != OMC_OK) return r;
    return omc_bw_end_chunk(f, geom);
}

static OMC_U32 omc_bw_shared_count(const OMC_Mesh *mesh)
{
    OMC_U16 si;
    if (mesh->shared_vertex_count > 0UL) return mesh->shared_vertex_count;
    si = 0U;
    while (si < mesh->submesh_count) {
        if (mesh->submeshes[si].use_shared_vertices) return mesh->vertex_count;
        si++;
    }
    return 0UL;
}

static OMC_U8 omc_bw_submesh_needs_32(const OMC_Mesh *mesh, const OMC_Submesh *sm)
{
    OMC_U32 i;
    if (sm->use_32bit_indices) return 1U;
    i = sm->index_start;
    while (i < sm->index_start + sm->index_count && i < mesh->index_count) {
        if (mesh->indices[i] > 65535UL) return 1U;
        i++;
    }
    return 0U;
}

static int omc_bw_write_bone_assignment_payload(FILE *f, const OMC_BoneAssignment *ba)
{
    int r;
    r = omc_bw_u32(f, ba->vertex_index); if (r != OMC_OK) return r;
    r = omc_bw_u16(f, ba->bone_index); if (r != OMC_OK) return r;
    return omc_bw_f32(f, ba->weight);
}

static int omc_bw_write_submesh_bones_binary(FILE *f, const OMC_Mesh *mesh, OMC_U16 submesh_index)
{
    OMC_U32 i;
    long ch;
    int r;
    i = 0UL;
    while (i < mesh->submesh_bone_assignment_count) {
        if (mesh->submesh_bone_assignments[i].submesh_index == submesh_index) {
            ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_SUBMESH_BONE_ASSIGNMENT);
            if (ch < 0L) return OMC_ERR_IO;
            r = omc_bw_write_bone_assignment_payload(f, &mesh->submesh_bone_assignments[i].assignment); if (r != OMC_OK) return r;
            r = omc_bw_end_chunk(f, ch); if (r != OMC_OK) return r;
        }
        i++;
    }
    return OMC_OK;
}

static int omc_bw_write_mesh_bones_binary(FILE *f, const OMC_Mesh *mesh)
{
    OMC_U32 i;
    long ch;
    int r;
    i = 0UL;
    while (i < mesh->bone_assignment_count) {
        ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_MESH_BONE_ASSIGNMENT);
        if (ch < 0L) return OMC_ERR_IO;
        r = omc_bw_write_bone_assignment_payload(f, &mesh->bone_assignments[i]); if (r != OMC_OK) return r;
        r = omc_bw_end_chunk(f, ch); if (r != OMC_OK) return r;
        i++;
    }
    return OMC_OK;
}

static int omc_bw_write_texture_aliases_binary(FILE *f, const OMC_Mesh *mesh, OMC_U16 submesh_index)
{
    OMC_U32 i;
    long ch;
    int r;
    i = 0UL;
    while (i < mesh->texture_alias_count) {
        if (mesh->texture_aliases[i].submesh_index == submesh_index) {
            ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_SUBMESH_TEXTURE_ALIAS);
            if (ch < 0L) return OMC_ERR_IO;
            r = omc_bw_string(f, mesh->texture_aliases[i].alias); if (r != OMC_OK) return r;
            r = omc_bw_string(f, mesh->texture_aliases[i].texture); if (r != OMC_OK) return r;
            r = omc_bw_end_chunk(f, ch); if (r != OMC_OK) return r;
        }
        i++;
    }
    return OMC_OK;
}

static int omc_bw_write_submesh_binary(FILE *f, const OMC_Mesh *mesh, OMC_U16 submesh_index)
{
    const OMC_Submesh *sm;
    long ch;
    long op;
    OMC_U32 i;
    OMC_U8 idx32;
    int r;

    sm = &mesh->submeshes[submesh_index];
    idx32 = omc_bw_submesh_needs_32(mesh, sm);
    ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_SUBMESH);
    if (ch < 0L) return OMC_ERR_IO;
    r = omc_bw_string(f, sm->material[0] ? sm->material : "BaseWhite"); if (r != OMC_OK) return r;
    r = omc_bw_u8(f, sm->use_shared_vertices); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, sm->index_count); if (r != OMC_OK) return r;
    r = omc_bw_u8(f, idx32); if (r != OMC_OK) return r;
    i = sm->index_start;
    while (i < sm->index_start + sm->index_count && i < mesh->index_count) {
        if (idx32) r = omc_bw_u32(f, mesh->indices[i]);
        else r = omc_bw_u16(f, (OMC_U16)mesh->indices[i]);
        if (r != OMC_OK) return r;
        i++;
    }
    if (!sm->use_shared_vertices || sm->has_dedicated_geometry) {
        r = omc_bw_write_geometry(f, mesh, sm->vertex_start, sm->vertex_count); if (r != OMC_OK) return r;
    }
    r = omc_bw_write_texture_aliases_binary(f, mesh, submesh_index); if (r != OMC_OK) return r;
    op = omc_bw_begin_chunk(f, OMC_BW_CHUNK_SUBMESH_OPERATION);
    if (op < 0L) return OMC_ERR_IO;
    r = omc_bw_u16(f, sm->operation_type ? sm->operation_type : (OMC_U16)OMC_OP_TRIANGLE_LIST); if (r != OMC_OK) return r;
    r = omc_bw_end_chunk(f, op); if (r != OMC_OK) return r;
    r = omc_bw_write_submesh_bones_binary(f, mesh, submesh_index); if (r != OMC_OK) return r;
    return omc_bw_end_chunk(f, ch);
}

static int omc_bw_write_skeleton_binary(FILE *f, const OMC_Mesh *mesh)
{
    long ch;
    int r;
    if (!mesh->skeleton_name[0]) return OMC_OK;
    ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_SKELETON_LINK);
    if (ch < 0L) return OMC_ERR_IO;
    r = omc_bw_string(f, mesh->skeleton_name); if (r != OMC_OK) return r;
    return omc_bw_end_chunk(f, ch);
}

static int omc_bw_write_bounds_binary(FILE *f, const OMC_Mesh *mesh)
{
    OMC_Vec3 mn;
    OMC_Vec3 mx;
    OMC_Fixed radius;
    long ch;
    int r;
    omc_bw_get_bounds(mesh, &mn, &mx, &radius);
    ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_MESH_BOUNDS);
    if (ch < 0L) return OMC_ERR_IO;
    r = omc_bw_f32(f, mn.x); if (r != OMC_OK) return r;
    r = omc_bw_f32(f, mn.y); if (r != OMC_OK) return r;
    r = omc_bw_f32(f, mn.z); if (r != OMC_OK) return r;
    r = omc_bw_f32(f, mx.x); if (r != OMC_OK) return r;
    r = omc_bw_f32(f, mx.y); if (r != OMC_OK) return r;
    r = omc_bw_f32(f, mx.z); if (r != OMC_OK) return r;
    r = omc_bw_f32(f, radius); if (r != OMC_OK) return r;
    return omc_bw_end_chunk(f, ch);
}

static int omc_bw_write_name_table_binary(FILE *f, const OMC_Mesh *mesh)
{
    long nt;
    long el;
    OMC_U16 i;
    int any;
    int r;
    any = 0;
    i = 0U;
    while (i < mesh->submesh_count) { if (mesh->submeshes[i].name[0]) any = 1; i++; }
    if (!any) return OMC_OK;
    nt = omc_bw_begin_chunk(f, OMC_BW_CHUNK_SUBMESH_NAME_TABLE);
    if (nt < 0L) return OMC_ERR_IO;
    i = 0U;
    while (i < mesh->submesh_count) {
        if (mesh->submeshes[i].name[0]) {
            el = omc_bw_begin_chunk(f, OMC_BW_CHUNK_SUBMESH_NAME_TABLE_ELEMENT);
            if (el < 0L) return OMC_ERR_IO;
            r = omc_bw_u16(f, i); if (r != OMC_OK) return r;
            r = omc_bw_string(f, mesh->submeshes[i].name); if (r != OMC_OK) return r;
            r = omc_bw_end_chunk(f, el); if (r != OMC_OK) return r;
        }
        i++;
    }
    return omc_bw_end_chunk(f, nt);
}

static OMC_U8 omc_bw_lod_submesh_needs_32(const OMC_Mesh *mesh, const OMC_LodLevel *lod, OMC_U16 submesh_index)
{
    OMC_U32 i;
    OMC_U32 end;
    const OMC_LodFace *lf;
    end = lod->face_start + lod->face_count;
    i = lod->face_start;
    while (i < end && i < mesh->lod_face_count) {
        lf = &mesh->lod_faces[i];
        if (lf->submesh_index == submesh_index) {
            if (lf->v1 > 65535UL || lf->v2 > 65535UL || lf->v3 > 65535UL) return 1U;
        }
        i++;
    }
    return 0U;
}

static OMC_U32 omc_bw_lod_submesh_face_count(const OMC_Mesh *mesh, const OMC_LodLevel *lod, OMC_U16 submesh_index)
{
    OMC_U32 i;
    OMC_U32 end;
    OMC_U32 n;
    end = lod->face_start + lod->face_count;
    i = lod->face_start;
    n = 0UL;
    while (i < end && i < mesh->lod_face_count) {
        if (mesh->lod_faces[i].submesh_index == submesh_index) n++;
        i++;
    }
    return n;
}

static int omc_bw_write_lod_binary(FILE *f, const OMC_Mesh *mesh)
{
    long top;
    long ch;
    OMC_U16 li;
    OMC_U16 si;
    OMC_U16 declared;
    OMC_U32 i;
    OMC_U32 end;
    OMC_U32 num_indices;
    OMC_U8 idx32;
    const OMC_LodLevel *lod;
    const OMC_LodFace *lf;
    int r;
    if (mesh->lod_level_count == 0U) return OMC_OK;
    top = omc_bw_begin_chunk(f, OMC_BW_CHUNK_LOD_LEVEL);
    if (top < 0L) return OMC_ERR_IO;
    r = omc_bw_string(f, mesh->lod_strategy[0] ? mesh->lod_strategy : "Distance"); if (r != OMC_OK) return r;
    declared = mesh->lod_declared_levels ? mesh->lod_declared_levels : (OMC_U16)(mesh->lod_level_count + 1U);
    r = omc_bw_u16(f, declared); if (r != OMC_OK) return r;
    li = 0U;
    while (li < mesh->lod_level_count) {
        lod = &mesh->lod_levels[li];
        if (lod->manual) {
            ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_LOD_MANUAL);
            if (ch < 0L) return OMC_ERR_IO;
            r = omc_bw_f32(f, lod->value); if (r != OMC_OK) return r;
            r = omc_bw_string(f, lod->mesh_name); if (r != OMC_OK) return r;
            r = omc_bw_end_chunk(f, ch); if (r != OMC_OK) return r;
        } else {
            ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_LOD_GENERATED);
            if (ch < 0L) return OMC_ERR_IO;
            r = omc_bw_f32(f, lod->value); if (r != OMC_OK) return r;
            si = 0U;
            while (si < mesh->submesh_count) {
                num_indices = omc_bw_lod_submesh_face_count(mesh, lod, si) * 3UL;
                idx32 = omc_bw_lod_submesh_needs_32(mesh, lod, si);
                r = omc_bw_u32(f, num_indices); if (r != OMC_OK) return r;
                r = omc_bw_u32(f, 0UL); if (r != OMC_OK) return r;
                r = omc_bw_u32(f, 0xffffffffUL); if (r != OMC_OK) return r;
                r = omc_bw_u8(f, idx32); if (r != OMC_OK) return r;
                r = omc_bw_u32(f, num_indices); if (r != OMC_OK) return r;
                end = lod->face_start + lod->face_count;
                i = lod->face_start;
                while (i < end && i < mesh->lod_face_count) {
                    lf = &mesh->lod_faces[i];
                    if (lf->submesh_index == si) {
                        if (idx32) { r = omc_bw_u32(f, lf->v1); if (r != OMC_OK) return r; }
                        else { r = omc_bw_u16(f, (OMC_U16)lf->v1); if (r != OMC_OK) return r; }
                        if (idx32) { r = omc_bw_u32(f, lf->v2); if (r != OMC_OK) return r; }
                        else { r = omc_bw_u16(f, (OMC_U16)lf->v2); if (r != OMC_OK) return r; }
                        if (idx32) { r = omc_bw_u32(f, lf->v3); if (r != OMC_OK) return r; }
                        else { r = omc_bw_u16(f, (OMC_U16)lf->v3); if (r != OMC_OK) return r; }
                    }
                    i++;
                }
                si++;
            }
            r = omc_bw_end_chunk(f, ch); if (r != OMC_OK) return r;
        }
        li++;
    }
    return omc_bw_end_chunk(f, top);
}

static OMC_U16 omc_bw_target_to_raw(OMC_U8 target_is_submesh, OMC_U16 target_index)
{
    if (!target_is_submesh) return 0U;
    return (OMC_U16)(target_index + 1U);
}

static unsigned long omc_bw_target_count(const OMC_Mesh *mesh, OMC_U8 target_is_submesh, OMC_U16 target_index)
{
    const OMC_Submesh *sm;
    if (!target_is_submesh) return (unsigned long)(mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count);
    if (target_index >= mesh->submesh_count) return 0UL;
    sm = &mesh->submeshes[target_index];
    if (!sm->use_shared_vertices) return (unsigned long)sm->vertex_count;
    return (unsigned long)(mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count);
}

static const OMC_Vertex *omc_bw_target_vertex(const OMC_Mesh *mesh, OMC_U8 target_is_submesh, OMC_U16 target_index, OMC_U32 local)
{
    const OMC_Submesh *sm;
    OMC_U32 actual;
    if (!target_is_submesh) {
        if (local < mesh->vertex_count) return &mesh->vertices[local];
        return (const OMC_Vertex *)0;
    }
    if (target_index >= mesh->submesh_count) return (const OMC_Vertex *)0;
    sm = &mesh->submeshes[target_index];
    if (sm->use_shared_vertices) actual = local;
    else actual = sm->vertex_start + local;
    if (actual < mesh->vertex_count) return &mesh->vertices[actual];
    return (const OMC_Vertex *)0;
}

static OMC_U8 omc_bw_pose_includes_normals(const OMC_Mesh *mesh, const OMC_Pose *pose)
{
    OMC_U32 i;
    i = pose->offset_start;
    while (i < pose->offset_start + pose->offset_count && i < mesh->pose_offset_count) {
        if (mesh->pose_offsets[i].has_normal) return 1U;
        i++;
    }
    return 0U;
}

static int omc_bw_write_poses_binary(FILE *f, const OMC_Mesh *mesh)
{
    long top;
    long pose_ch;
    long pv_ch;
    OMC_U16 pi;
    OMC_U32 oi;
    OMC_U8 includes_normals;
    const OMC_Pose *pose;
    const OMC_PoseOffset *off;
    int r;
    if (mesh->pose_count == 0U) return OMC_OK;
    top = omc_bw_begin_chunk(f, OMC_BW_CHUNK_POSES);
    if (top < 0L) return OMC_ERR_IO;
    pi = 0U;
    while (pi < mesh->pose_count) {
        pose = &mesh->poses[pi];
        includes_normals = omc_bw_pose_includes_normals(mesh, pose);
        pose_ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_POSE);
        if (pose_ch < 0L) return OMC_ERR_IO;
        r = omc_bw_string(f, pose->name); if (r != OMC_OK) return r;
        r = omc_bw_u16(f, omc_bw_target_to_raw(pose->target_is_submesh, pose->target_index)); if (r != OMC_OK) return r;
        r = omc_bw_u8(f, includes_normals); if (r != OMC_OK) return r;
        oi = pose->offset_start;
        while (oi < pose->offset_start + pose->offset_count && oi < mesh->pose_offset_count) {
            off = &mesh->pose_offsets[oi];
            pv_ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_POSE_VERTEX);
            if (pv_ch < 0L) return OMC_ERR_IO;
            r = omc_bw_u32(f, off->vertex_index); if (r != OMC_OK) return r;
            r = omc_bw_f32(f, off->offset.x); if (r != OMC_OK) return r;
            r = omc_bw_f32(f, off->offset.y); if (r != OMC_OK) return r;
            r = omc_bw_f32(f, off->offset.z); if (r != OMC_OK) return r;
            if (includes_normals) {
                r = omc_bw_f32(f, off->has_normal ? off->normal.x : 0); if (r != OMC_OK) return r;
                r = omc_bw_f32(f, off->has_normal ? off->normal.y : OMC_FIXED_ONE); if (r != OMC_OK) return r;
                r = omc_bw_f32(f, off->has_normal ? off->normal.z : 0); if (r != OMC_OK) return r;
            }
            r = omc_bw_end_chunk(f, pv_ch); if (r != OMC_OK) return r;
            oi++;
        }
        r = omc_bw_end_chunk(f, pose_ch); if (r != OMC_OK) return r;
        pi++;
    }
    return omc_bw_end_chunk(f, top);
}

static const OMC_MorphVertex *omc_bw_find_morph_vertex(const OMC_Mesh *mesh, const OMC_MorphKey *mk, OMC_U32 local)
{
    OMC_U32 i;
    const OMC_MorphVertex *mv;
    i = mk->vertex_start;
    while (i < mk->vertex_start + mk->vertex_count && i < mesh->morph_vertex_count) {
        mv = &mesh->morph_vertices[i];
        if (mv->local_index == local) return mv;
        i++;
    }
    return (const OMC_MorphVertex *)0;
}

static int omc_bw_write_morph_key_binary(FILE *f, const OMC_Mesh *mesh, const OMC_AnimationTrack *track, OMC_U32 morph_key_index)
{
    const OMC_MorphKey *mk;
    const OMC_MorphVertex *mv;
    const OMC_Vertex *base;
    long ch;
    unsigned long target_count;
    OMC_U32 local;
    OMC_Vec3 pos;
    OMC_Vec3 norm;
    int r;

    if (morph_key_index >= mesh->morph_key_count) return OMC_ERR_FORMAT;
    mk = &mesh->morph_keys[morph_key_index];
    target_count = omc_bw_target_count(mesh, track->target_is_submesh, track->target_index);
    if (target_count == 0UL) target_count = (unsigned long)mk->vertex_count;
    ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_ANIMATION_MORPH_KEYFRAME);
    if (ch < 0L) return OMC_ERR_IO;
    r = omc_bw_f32(f, mk->time); if (r != OMC_OK) return r;
    r = omc_bw_u8(f, mk->includes_normals); if (r != OMC_OK) return r;
    local = 0UL;
    while ((unsigned long)local < target_count) {
        pos.x = 0; pos.y = 0; pos.z = 0;
        norm.x = 0; norm.y = OMC_FIXED_ONE; norm.z = 0;
        base = omc_bw_target_vertex(mesh, track->target_is_submesh, track->target_index, local);
        if (base) { pos = base->position; norm = base->normal; }
        mv = omc_bw_find_morph_vertex(mesh, mk, local);
        if (mv) {
            pos = mv->position;
            if (mv->has_normal) norm = mv->normal;
        }
        r = omc_bw_f32(f, pos.x); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, pos.y); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, pos.z); if (r != OMC_OK) return r;
        if (mk->includes_normals) {
            r = omc_bw_f32(f, norm.x); if (r != OMC_OK) return r;
            r = omc_bw_f32(f, norm.y); if (r != OMC_OK) return r;
            r = omc_bw_f32(f, norm.z); if (r != OMC_OK) return r;
        }
        local++;
    }
    return omc_bw_end_chunk(f, ch);
}

static int omc_bw_write_pose_keyframe_binary(FILE *f, const OMC_Mesh *mesh, const OMC_AnimationTrack *track, OMC_U32 *pki)
{
    long kf_ch;
    long ref_ch;
    OMC_Fixed time;
    const OMC_AnimationKey *key;
    OMC_U32 ki;
    int r;

    ki = *pki;
    if (ki >= mesh->key_count) return OMC_ERR_FORMAT;
    time = mesh->keys[ki].time;
    kf_ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_ANIMATION_POSE_KEYFRAME);
    if (kf_ch < 0L) return OMC_ERR_IO;
    r = omc_bw_f32(f, time); if (r != OMC_OK) return r;
    while (ki < track->key_start + track->key_count && ki < mesh->key_count && mesh->keys[ki].time == time) {
        key = &mesh->keys[ki];
        ref_ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_ANIMATION_POSE_REF);
        if (ref_ch < 0L) return OMC_ERR_IO;
        r = omc_bw_u16(f, key->pose_index); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, key->influence); if (r != OMC_OK) return r;
        r = omc_bw_end_chunk(f, ref_ch); if (r != OMC_OK) return r;
        ki++;
    }
    *pki = ki;
    return omc_bw_end_chunk(f, kf_ch);
}

static int omc_bw_write_animations_binary(FILE *f, const OMC_Mesh *mesh)
{
    long top;
    long anim_ch;
    long base_ch;
    long track_ch;
    OMC_U16 ai;
    OMC_U32 ti;
    OMC_U32 ki;
    OMC_U32 mi;
    const OMC_Animation *anim;
    const OMC_AnimationTrack *track;
    int r;
    if (mesh->animation_count == 0U) return OMC_OK;
    top = omc_bw_begin_chunk(f, OMC_BW_CHUNK_ANIMATIONS);
    if (top < 0L) return OMC_ERR_IO;
    ai = 0U;
    while (ai < mesh->animation_count) {
        anim = &mesh->animations[ai];
        anim_ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_ANIMATION);
        if (anim_ch < 0L) return OMC_ERR_IO;
        r = omc_bw_string(f, anim->name); if (r != OMC_OK) return r;
        r = omc_bw_f32(f, anim->length); if (r != OMC_OK) return r;
        if (anim->has_baseinfo) {
            base_ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_ANIMATION_BASEINFO);
            if (base_ch < 0L) return OMC_ERR_IO;
            r = omc_bw_string(f, anim->base_animation_name); if (r != OMC_OK) return r;
            r = omc_bw_f32(f, anim->base_keyframe_time); if (r != OMC_OK) return r;
            r = omc_bw_end_chunk(f, base_ch); if (r != OMC_OK) return r;
        }
        ti = anim->track_start;
        while (ti < anim->track_start + anim->track_count && ti < mesh->track_count) {
            track = &mesh->tracks[ti];
            track_ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_ANIMATION_TRACK);
            if (track_ch < 0L) return OMC_ERR_IO;
            r = omc_bw_u16(f, track->is_pose_track ? 2U : 1U); if (r != OMC_OK) return r;
            r = omc_bw_u16(f, omc_bw_target_to_raw(track->target_is_submesh, track->target_index)); if (r != OMC_OK) return r;
            if (track->is_pose_track) {
                ki = track->key_start;
                while (ki < track->key_start + track->key_count && ki < mesh->key_count) {
                    r = omc_bw_write_pose_keyframe_binary(f, mesh, track, &ki);
                    if (r != OMC_OK) return r;
                }
            } else {
                mi = track->morph_key_start;
                while (mi < track->morph_key_start + track->morph_key_count && mi < mesh->morph_key_count) {
                    r = omc_bw_write_morph_key_binary(f, mesh, track, mi);
                    if (r != OMC_OK) return r;
                    mi++;
                }
            }
            r = omc_bw_end_chunk(f, track_ch); if (r != OMC_OK) return r;
            ti++;
        }
        r = omc_bw_end_chunk(f, anim_ch); if (r != OMC_OK) return r;
        ai++;
    }
    return omc_bw_end_chunk(f, top);
}

static int omc_bw_write_extremes_binary(FILE *f, const OMC_Mesh *mesh)
{
    OMC_U16 si;
    OMC_U32 i;
    int any;
    long ch;
    int r;
    si = 0U;
    while (si < mesh->submesh_count) {
        any = 0;
        i = 0UL;
        while (i < mesh->extreme_count) { if (mesh->extremes[i].submesh_index == si) any = 1; i++; }
        if (any) {
            ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_TABLE_EXTREMES);
            if (ch < 0L) return OMC_ERR_IO;
            r = omc_bw_u16(f, si); if (r != OMC_OK) return r;
            i = 0UL;
            while (i < mesh->extreme_count) {
                if (mesh->extremes[i].submesh_index == si) {
                    r = omc_bw_f32(f, mesh->extremes[i].position.x); if (r != OMC_OK) return r;
                    r = omc_bw_f32(f, mesh->extremes[i].position.y); if (r != OMC_OK) return r;
                    r = omc_bw_f32(f, mesh->extremes[i].position.z); if (r != OMC_OK) return r;
                }
                i++;
            }
            r = omc_bw_end_chunk(f, ch); if (r != OMC_OK) return r;
        }
        si++;
    }
    return OMC_OK;
}

typedef struct OMC_BWEdgeGroupInfoTag {
    OMC_U32 vertex_set;
    OMC_U32 tri_start;
    OMC_U32 tri_count;
} OMC_BWEdgeGroupInfo;

static int omc_bw_patch_u32(FILE *f, long pos, OMC_U32 v)
{
    long cur;
    int r;
    cur = ftell(f);
    if (cur < 0L || pos < 0L) return OMC_ERR_IO;
    if (fseek(f, pos, SEEK_SET) != 0) return OMC_ERR_IO;
    r = omc_bw_u32(f, v);
    if (r != OMC_OK) return r;
    if (fseek(f, cur, SEEK_SET) != 0) return OMC_ERR_IO;
    return OMC_OK;
}

static int omc_bw_submesh_has_triangle_edges(const OMC_Submesh *sm)
{
    if (!sm) return 0;
    if ((sm->operation_type ? sm->operation_type : (OMC_U8)OMC_OP_TRIANGLE_LIST) != (OMC_U8)OMC_OP_TRIANGLE_LIST) return 0;
    return sm->index_count >= 3UL;
}

static OMC_U32 omc_bw_edge_vertex_set(const OMC_Mesh *mesh, const OMC_Submesh *sm, OMC_U16 si)
{
    if (omc_bw_shared_count(mesh) > 0UL) {
        if (sm->use_shared_vertices) return 0UL;
        return (OMC_U32)si + 1UL;
    }
    return (OMC_U32)si;
}

static OMC_U32 omc_bw_edge_actual_vertex(const OMC_Submesh *sm, OMC_U32 local)
{
    if (!sm) return local;
    if (sm->use_shared_vertices) return local;
    return sm->vertex_start + local;
}

static int omc_bw_edge_group_find(const OMC_BWEdgeGroupInfo *groups, OMC_U16 group_count, OMC_U32 vertex_set)
{
    OMC_U16 i;
    i = 0U;
    while (i < group_count) {
        if (groups[i].vertex_set == vertex_set) return (int)i;
        i++;
    }
    return -1;
}

static OMC_U16 omc_bw_collect_edge_groups(const OMC_Mesh *mesh, OMC_BWEdgeGroupInfo *groups)
{
    OMC_U16 si;
    OMC_U16 group_count;
    OMC_U32 vertex_set;
    const OMC_Submesh *sm;

    group_count = 0U;
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        if (omc_bw_submesh_has_triangle_edges(sm)) {
            vertex_set = omc_bw_edge_vertex_set(mesh, sm, si);
            if (omc_bw_edge_group_find(groups, group_count, vertex_set) < 0 && group_count < (OMC_U16)(OMC_MAX_SUBMESHES + 1)) {
                groups[group_count].vertex_set = vertex_set;
                groups[group_count].tri_start = 0UL;
                groups[group_count].tri_count = 0UL;
                group_count++;
            }
        }
        si++;
    }
    return group_count;
}

static OMC_U32 omc_bw_count_triangles_for_vertex_set(const OMC_Mesh *mesh, OMC_U32 vertex_set)
{
    OMC_U16 si;
    OMC_U32 n;
    const OMC_Submesh *sm;
    n = 0UL;
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        if (omc_bw_submesh_has_triangle_edges(sm) && omc_bw_edge_vertex_set(mesh, sm, si) == vertex_set) {
            n += sm->index_count / 3UL;
        }
        si++;
    }
    return n;
}

static OMC_U32 omc_bw_prepare_edge_groups(const OMC_Mesh *mesh, OMC_BWEdgeGroupInfo *groups, OMC_U16 group_count)
{
    OMC_U16 gi;
    OMC_U32 total;
    total = 0UL;
    gi = 0U;
    while (gi < group_count) {
        groups[gi].tri_start = total;
        groups[gi].tri_count = omc_bw_count_triangles_for_vertex_set(mesh, groups[gi].vertex_set);
        total += groups[gi].tri_count;
        gi++;
    }
    return total;
}

static OMC_Vec3 omc_bw_triangle_normal(const OMC_Mesh *mesh, OMC_U32 a, OMC_U32 b, OMC_U32 c)
{
    OMC_Vec3 n;
    OMC_Vec3 ab;
    OMC_Vec3 ac;
    n.x = 0; n.y = OMC_FIXED_ONE; n.z = 0;
    if (a >= mesh->vertex_count || b >= mesh->vertex_count || c >= mesh->vertex_count) return n;
    ab = omc_bw_vec3_sub(mesh->vertices[b].position, mesh->vertices[a].position);
    ac = omc_bw_vec3_sub(mesh->vertices[c].position, mesh->vertices[a].position);
    return omc_bw_vec3_normalize(omc_bw_vec3_cross(ab, ac));
}

static int omc_bw_write_edge_triangle_record(FILE *f, const OMC_Mesh *mesh, const OMC_Submesh *sm, OMC_U16 si, OMC_U32 vertex_set, OMC_U32 a, OMC_U32 b, OMC_U32 c)
{
    OMC_U32 aa;
    OMC_U32 bb;
    OMC_U32 cc;
    OMC_Vec3 n;
    OMC_Fixed plane_w;
    int r;

    aa = omc_bw_edge_actual_vertex(sm, a);
    bb = omc_bw_edge_actual_vertex(sm, b);
    cc = omc_bw_edge_actual_vertex(sm, c);
    n = omc_bw_triangle_normal(mesh, aa, bb, cc);
    plane_w = 0;
    if (aa < mesh->vertex_count) plane_w = -omc_bw_vec3_dot(n, mesh->vertices[aa].position);

    r = omc_bw_u32(f, (OMC_U32)si); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, vertex_set); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, a); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, b); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, c); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, aa); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, bb); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, cc); if (r != OMC_OK) return r;
    r = omc_bw_f32(f, n.x); if (r != OMC_OK) return r;
    r = omc_bw_f32(f, n.y); if (r != OMC_OK) return r;
    r = omc_bw_f32(f, n.z); if (r != OMC_OK) return r;
    return omc_bw_f32(f, plane_w);
}

static int omc_bw_triangle_has_edge(OMC_U32 a, OMC_U32 b, OMC_U32 c, OMC_U32 e0, OMC_U32 e1)
{
    if ((a == e0 && b == e1) || (a == e1 && b == e0)) return 1;
    if ((b == e0 && c == e1) || (b == e1 && c == e0)) return 1;
    if ((c == e0 && a == e1) || (c == e1 && a == e0)) return 1;
    return 0;
}

static int omc_bw_find_neighbor_triangle(const OMC_Mesh *mesh, const OMC_BWEdgeGroupInfo *group, OMC_U32 self_tri, OMC_U32 e0, OMC_U32 e1, OMC_U32 *neighbor)
{
    OMC_U16 si;
    OMC_U32 j;
    OMC_U32 tri;
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    const OMC_Submesh *sm;

    tri = group->tri_start;
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        if (omc_bw_submesh_has_triangle_edges(sm) && omc_bw_edge_vertex_set(mesh, sm, si) == group->vertex_set) {
            j = sm->index_start;
            while (j + 2UL < sm->index_start + sm->index_count && j + 2UL < mesh->index_count) {
                a = mesh->indices[j];
                b = mesh->indices[j + 1UL];
                c = mesh->indices[j + 2UL];
                if (tri != self_tri && omc_bw_triangle_has_edge(a, b, c, e0, e1)) {
                    *neighbor = tri;
                    return 1;
                }
                tri++;
                j += 3UL;
            }
        }
        si++;
    }
    *neighbor = 0xffffffffUL;
    return 0;
}

static int omc_bw_edge_mesh_is_closed(const OMC_Mesh *mesh, const OMC_BWEdgeGroupInfo *groups, OMC_U16 group_count)
{
    OMC_U16 gi;
    OMC_U16 si;
    OMC_U32 j;
    OMC_U32 tri;
    OMC_U32 neighbor;
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    const OMC_Submesh *sm;

    gi = 0U;
    while (gi < group_count) {
        tri = groups[gi].tri_start;
        si = 0U;
        while (si < mesh->submesh_count) {
            sm = &mesh->submeshes[si];
            if (omc_bw_submesh_has_triangle_edges(sm) && omc_bw_edge_vertex_set(mesh, sm, si) == groups[gi].vertex_set) {
                j = sm->index_start;
                while (j + 2UL < sm->index_start + sm->index_count && j + 2UL < mesh->index_count) {
                    a = mesh->indices[j];
                    b = mesh->indices[j + 1UL];
                    c = mesh->indices[j + 2UL];
                    if (!omc_bw_find_neighbor_triangle(mesh, &groups[gi], tri, a, b, &neighbor)) return 0;
                    if (!omc_bw_find_neighbor_triangle(mesh, &groups[gi], tri, b, c, &neighbor)) return 0;
                    if (!omc_bw_find_neighbor_triangle(mesh, &groups[gi], tri, c, a, &neighbor)) return 0;
                    tri++;
                    j += 3UL;
                }
            }
            si++;
        }
        gi++;
    }
    return 1;
}

static int omc_bw_write_edge_record(FILE *f, OMC_U32 tri0, OMC_U32 tri1, OMC_U32 v0, OMC_U32 v1, OMC_U32 shared0, OMC_U32 shared1, OMC_U8 degenerate)
{
    int r;
    r = omc_bw_u32(f, tri0); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, tri1); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, v0); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, v1); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, shared0); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, shared1); if (r != OMC_OK) return r;
    return omc_bw_u8(f, degenerate);
}

static int omc_bw_write_edge_candidate(FILE *f, const OMC_Mesh *mesh, const OMC_Submesh *sm, const OMC_BWEdgeGroupInfo *group, OMC_U32 tri_index, OMC_U32 e0, OMC_U32 e1, OMC_U32 *edge_count)
{
    OMC_U32 neighbor;
    OMC_U32 shared0;
    OMC_U32 shared1;
    int found;
    int r;

    found = omc_bw_find_neighbor_triangle(mesh, group, tri_index, e0, e1, &neighbor);
    if (found && neighbor < tri_index) return OMC_OK;
    shared0 = omc_bw_edge_actual_vertex(sm, e0);
    shared1 = omc_bw_edge_actual_vertex(sm, e1);
    r = omc_bw_write_edge_record(f, tri_index, found ? neighbor : 0xffffffffUL, e0, e1, shared0, shared1, found ? 0U : 1U);
    if (r != OMC_OK) return r;
    *edge_count = *edge_count + 1UL;
    return OMC_OK;
}

static int omc_bw_write_edge_group_binary(FILE *f, const OMC_Mesh *mesh, const OMC_BWEdgeGroupInfo *group)
{
    long ch;
    long edge_count_pos;
    OMC_U16 si;
    OMC_U32 j;
    OMC_U32 tri_index;
    OMC_U32 edge_count;
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    const OMC_Submesh *sm;
    int r;

    ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_EDGE_GROUP);
    if (ch < 0L) return OMC_ERR_IO;
    r = omc_bw_u32(f, group->vertex_set); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, group->tri_start); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, group->tri_count); if (r != OMC_OK) return r;
    edge_count_pos = ftell(f);
    if (edge_count_pos < 0L) return OMC_ERR_IO;
    r = omc_bw_u32(f, 0UL); if (r != OMC_OK) return r;

    edge_count = 0UL;
    tri_index = group->tri_start;
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        if (omc_bw_submesh_has_triangle_edges(sm) && omc_bw_edge_vertex_set(mesh, sm, si) == group->vertex_set) {
            j = sm->index_start;
            while (j + 2UL < sm->index_start + sm->index_count && j + 2UL < mesh->index_count) {
                a = mesh->indices[j];
                b = mesh->indices[j + 1UL];
                c = mesh->indices[j + 2UL];
                r = omc_bw_write_edge_candidate(f, mesh, sm, group, tri_index, a, b, &edge_count); if (r != OMC_OK) return r;
                r = omc_bw_write_edge_candidate(f, mesh, sm, group, tri_index, b, c, &edge_count); if (r != OMC_OK) return r;
                r = omc_bw_write_edge_candidate(f, mesh, sm, group, tri_index, c, a, &edge_count); if (r != OMC_OK) return r;
                tri_index++;
                j += 3UL;
            }
        }
        si++;
    }

    r = omc_bw_patch_u32(f, edge_count_pos, edge_count); if (r != OMC_OK) return r;
    return omc_bw_end_chunk(f, ch);
}

static int omc_bw_write_edge_lists_binary(FILE *f, const OMC_Mesh *mesh)
{
    OMC_BWEdgeGroupInfo groups[OMC_MAX_SUBMESHES + 1];
    OMC_U16 group_count;
    OMC_U16 gi;
    OMC_U16 si;
    OMC_U32 total_triangles;
    OMC_U32 j;
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    long top;
    long lod;
    const OMC_Submesh *sm;
    int r;

    group_count = omc_bw_collect_edge_groups(mesh, groups);
    if (group_count == 0U) return OMC_OK;
    total_triangles = omc_bw_prepare_edge_groups(mesh, groups, group_count);
    if (total_triangles == 0UL) return OMC_OK;

    top = omc_bw_begin_chunk(f, OMC_BW_CHUNK_EDGE_LISTS);
    if (top < 0L) return OMC_ERR_IO;
    lod = omc_bw_begin_chunk(f, OMC_BW_CHUNK_EDGE_LIST_LOD);
    if (lod < 0L) return OMC_ERR_IO;
    r = omc_bw_u16(f, 0U); if (r != OMC_OK) return r;
    r = omc_bw_u8(f, 0U); if (r != OMC_OK) return r;
    r = omc_bw_u8(f, (OMC_U8)omc_bw_edge_mesh_is_closed(mesh, groups, group_count)); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, total_triangles); if (r != OMC_OK) return r;
    r = omc_bw_u32(f, (OMC_U32)group_count); if (r != OMC_OK) return r;

    gi = 0U;
    while (gi < group_count) {
        si = 0U;
        while (si < mesh->submesh_count) {
            sm = &mesh->submeshes[si];
            if (omc_bw_submesh_has_triangle_edges(sm) && omc_bw_edge_vertex_set(mesh, sm, si) == groups[gi].vertex_set) {
                j = sm->index_start;
                while (j + 2UL < sm->index_start + sm->index_count && j + 2UL < mesh->index_count) {
                    a = mesh->indices[j];
                    b = mesh->indices[j + 1UL];
                    c = mesh->indices[j + 2UL];
                    r = omc_bw_write_edge_triangle_record(f, mesh, sm, si, groups[gi].vertex_set, a, b, c);
                    if (r != OMC_OK) return r;
                    j += 3UL;
                }
            }
            si++;
        }
        gi++;
    }

    gi = 0U;
    while (gi < group_count) {
        r = omc_bw_write_edge_group_binary(f, mesh, &groups[gi]);
        if (r != OMC_OK) return r;
        gi++;
    }

    r = omc_bw_end_chunk(f, lod); if (r != OMC_OK) return r;
    return omc_bw_end_chunk(f, top);
}

static int omc_bw_write_mesh_binary_payload(FILE *f, const OMC_Mesh *mesh)
{
    OMC_U16 si;
    OMC_U32 shared_count;
    OMC_U8 skel;
    int r;

    skel = (OMC_U8)((mesh->skeleton_name[0] || mesh->bone_assignment_count || mesh->submesh_bone_assignment_count) ? 1U : 0U);
    r = omc_bw_u8(f, skel); if (r != OMC_OK) return r;
    shared_count = omc_bw_shared_count(mesh);
    if (shared_count > 0UL) {
        r = omc_bw_write_geometry(f, mesh, 0UL, shared_count); if (r != OMC_OK) return r;
    }
    si = 0U;
    while (si < mesh->submesh_count) {
        r = omc_bw_write_submesh_binary(f, mesh, si); if (r != OMC_OK) return r;
        si++;
    }
    r = omc_bw_write_skeleton_binary(f, mesh); if (r != OMC_OK) return r;
    r = omc_bw_write_mesh_bones_binary(f, mesh); if (r != OMC_OK) return r;
    r = omc_bw_write_lod_binary(f, mesh); if (r != OMC_OK) return r;
    r = omc_bw_write_bounds_binary(f, mesh); if (r != OMC_OK) return r;
    r = omc_bw_write_name_table_binary(f, mesh); if (r != OMC_OK) return r;
    r = omc_bw_write_edge_lists_binary(f, mesh); if (r != OMC_OK) return r;
    r = omc_bw_write_poses_binary(f, mesh); if (r != OMC_OK) return r;
    r = omc_bw_write_animations_binary(f, mesh); if (r != OMC_OK) return r;
    r = omc_bw_write_extremes_binary(f, mesh); if (r != OMC_OK) return r;
    return OMC_OK;
}

int omc_save_mesh_binary_file(const OMC_Mesh *mesh, const char *path)
{
    FILE *f;
    long ch;
    long mesh_ch;
    int r;

    if (!mesh || !path) return OMC_ERR_NULL;
    r = omc_validate_mesh(mesh);
    if (r != OMC_OK) return r;
    f = fopen(path, "wb");
    if (!f) return OMC_ERR_IO;

    ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_HEADER);
    if (ch < 0L) { fclose(f); return OMC_ERR_IO; }
    r = omc_bw_string(f, "[MeshSerializer_v1.100]");
    if (r == OMC_OK) r = omc_bw_end_chunk(f, ch);
    if (r != OMC_OK) { fclose(f); return r; }

    mesh_ch = omc_bw_begin_chunk(f, OMC_BW_CHUNK_MESH);
    if (mesh_ch < 0L) { fclose(f); return OMC_ERR_IO; }
    r = omc_bw_write_mesh_binary_payload(f, mesh);
    if (r == OMC_OK) r = omc_bw_end_chunk(f, mesh_ch);
    if (ferror(f)) r = OMC_ERR_IO;
    fclose(f);
    return r;
}



static OMC_U32 omc_obj_vertex_index(const OMC_Submesh *sm, OMC_U32 local_index)
{
    OMC_U32 v;
    v = local_index + 1UL;
    if (sm && !sm->use_shared_vertices) v += sm->vertex_start;
    return v;
}

static int omc_obj_face_degenerate(OMC_U32 a, OMC_U32 b, OMC_U32 c)
{
    return a == b || b == c || a == c;
}

static void omc_obj_write_face(FILE *f, OMC_U32 a, OMC_U32 b, OMC_U32 c)
{
    if (omc_obj_face_degenerate(a, b, c)) return;
    fprintf(f, "f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n",
            (unsigned long)a, (unsigned long)a, (unsigned long)a,
            (unsigned long)b, (unsigned long)b, (unsigned long)b,
            (unsigned long)c, (unsigned long)c, (unsigned long)c);
}

static void omc_obj_write_line(FILE *f, OMC_U32 a, OMC_U32 b)
{
    if (a == b) return;
    fprintf(f, "l %lu %lu\n", (unsigned long)a, (unsigned long)b);
}

static void omc_obj_write_point(FILE *f, OMC_U32 a)
{
    fprintf(f, "p %lu\n", (unsigned long)a);
}

int omc_export_obj_file(const OMC_Mesh *mesh, const char *path)
{
    FILE *f;
    OMC_U32 i;
    OMC_U16 s;
    const OMC_Submesh *sm;
    const OMC_Vertex *v;

    if (!mesh || !path) return OMC_ERR_NULL;
    f = fopen(path, "wb");
    if (!f) return OMC_ERR_IO;
    fprintf(f, "# exported by ogre_mesh_codec_c89\n");
    i = 0UL;
    while (i < mesh->vertex_count) {
        v = &mesh->vertices[i];
        fprintf(f, "v "); omc_fprint_fixed(f, v->position.x);
        fprintf(f, " "); omc_fprint_fixed(f, v->position.y);
        fprintf(f, " "); omc_fprint_fixed(f, v->position.z);
        fprintf(f, "\n");
        i++;
    }
    i = 0UL;
    while (i < mesh->vertex_count) {
        v = &mesh->vertices[i];
        if ((v->flags & OMC_VTX_HAS_TEXCOORD) != 0U) {
            fprintf(f, "vt "); omc_fprint_fixed(f, v->texcoord[0].u);
            fprintf(f, " "); omc_fprint_fixed(f, v->texcoord[0].v);
            fprintf(f, "\n");
        } else {
            fprintf(f, "vt 0 0\n");
        }
        i++;
    }
    i = 0UL;
    while (i < mesh->vertex_count) {
        v = &mesh->vertices[i];
        if ((v->flags & OMC_VTX_HAS_NORMAL) != 0U) {
            fprintf(f, "vn "); omc_fprint_fixed(f, v->normal.x);
            fprintf(f, " "); omc_fprint_fixed(f, v->normal.y);
            fprintf(f, " "); omc_fprint_fixed(f, v->normal.z);
            fprintf(f, "\n");
        } else {
            fprintf(f, "vn 0 1 0\n");
        }
        i++;
    }

    s = 0U;
    while (s < mesh->submesh_count) {
        OMC_U32 rel;
        OMC_U32 a;
        OMC_U32 b;
        OMC_U32 c;
        sm = &mesh->submeshes[s];
        fprintf(f, "g %s\n", sm->name[0] ? sm->name : sm->material);
        rel = 0UL;
        if (sm->operation_type == (OMC_U8)OMC_OP_POINT_LIST ||
            (sm->operation_type >= (OMC_U8)OMC_OP_PATCH_1_CONTROL_POINT && sm->operation_type <= (OMC_U8)OMC_OP_PATCH_32_CONTROL_POINT)) {
            while (rel < sm->index_count && sm->index_start + rel < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel]);
                omc_obj_write_point(f, a);
                rel++;
            }
        } else if (sm->operation_type == (OMC_U8)OMC_OP_LINE_LIST) {
            while (rel + 1UL < sm->index_count && sm->index_start + rel + 1UL < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel]);
                b = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 1UL]);
                omc_obj_write_line(f, a, b);
                rel += 2UL;
            }
        } else if (sm->operation_type == (OMC_U8)OMC_OP_LINE_STRIP) {
            while (rel + 1UL < sm->index_count && sm->index_start + rel + 1UL < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel]);
                b = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 1UL]);
                omc_obj_write_line(f, a, b);
                rel++;
            }
        } else if (sm->operation_type == (OMC_U8)OMC_OP_LINE_LIST_ADJ) {
            while (rel + 2UL < sm->index_count && sm->index_start + rel + 2UL < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 1UL]);
                b = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 2UL]);
                omc_obj_write_line(f, a, b);
                rel += 4UL;
            }
        } else if (sm->operation_type == (OMC_U8)OMC_OP_LINE_STRIP_ADJ) {
            while (rel + 2UL < sm->index_count && sm->index_start + rel + 2UL < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 1UL]);
                b = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 2UL]);
                omc_obj_write_line(f, a, b);
                rel++;
            }
        } else if (sm->operation_type == (OMC_U8)OMC_OP_TRIANGLE_STRIP) {
            while (rel + 2UL < sm->index_count && sm->index_start + rel + 2UL < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel]);
                b = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 1UL]);
                c = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 2UL]);
                if ((rel & 1UL) != 0UL) omc_obj_write_face(f, b, a, c);
                else omc_obj_write_face(f, a, b, c);
                rel++;
            }
        } else if (sm->operation_type == (OMC_U8)OMC_OP_TRIANGLE_FAN) {
            if (sm->index_count >= 3UL && sm->index_start < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start]);
                rel = 1UL;
                while (rel + 1UL < sm->index_count && sm->index_start + rel + 1UL < mesh->index_count) {
                    b = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel]);
                    c = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 1UL]);
                    omc_obj_write_face(f, a, b, c);
                    rel++;
                }
            }
        } else if (sm->operation_type == (OMC_U8)OMC_OP_TRIANGLE_LIST_ADJ) {
            while (rel + 4UL < sm->index_count && sm->index_start + rel + 4UL < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel]);
                b = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 2UL]);
                c = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 4UL]);
                omc_obj_write_face(f, a, b, c);
                rel += 6UL;
            }
        } else if (sm->operation_type == (OMC_U8)OMC_OP_TRIANGLE_STRIP_ADJ) {
            while (rel + 4UL < sm->index_count && sm->index_start + rel + 4UL < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel]);
                b = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 2UL]);
                c = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 4UL]);
                if (((rel / 2UL) & 1UL) != 0UL) omc_obj_write_face(f, b, a, c);
                else omc_obj_write_face(f, a, b, c);
                rel += 2UL;
            }
        } else {
            while (rel + 2UL < sm->index_count && sm->index_start + rel + 2UL < mesh->index_count) {
                a = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel]);
                b = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 1UL]);
                c = omc_obj_vertex_index(sm, mesh->indices[sm->index_start + rel + 2UL]);
                omc_obj_write_face(f, a, b, c);
                rel += 3UL;
            }
        }
        s++;
    }

    fclose(f);
    return OMC_OK;
}
