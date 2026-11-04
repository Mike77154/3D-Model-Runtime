#include <stdio.h>
#include <string.h>
#include "p3_config.h"
#include "p3_error.h"
#include "p3_fileio.h"
#include "p3_fixed.h"
#include "p3_bam_reader.h"
#include "p3_bam_special.h"
#include "p3_bam_scene.h"
#include "p3_bam_ir.h"
#include "p3_p3m_writer.h"
#include "p3_bam_writer.h"

static p3_u8 file_buf[P3_MAX_FILE_BYTES];
static p3_u8 out_buf[P3_MAX_FILE_BYTES];
static char err_buf[256];

static const char *kind_name(p3_u8 kind) {
    switch (kind) {
    case P3_BAM_BOC_PUSH:
        return "push";
    case P3_BAM_BOC_POP:
        return "pop";
    case P3_BAM_BOC_ADJUNCT:
        return "adjunct";
    case P3_BAM_BOC_REMOVE:
        return "remove";
    case P3_BAM_BOC_FILE_DATA:
        return "file_data";
    default:
        return "unknown";
    }
}

static void print_fixed_value(p3_fixed value) {
    char buf[32];
    p3_fixed_to_str(value, buf);
    printf("%s", buf);
}

static void print_object_summary(const p3_bam_object *obj) {
    if (obj->class_kind == P3_BAM_CLASS_INTERNAL_NAME) {
        printf("name=\"%s\"", obj->u.internal_name);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_GEOM_VERTEX_COLUMN) {
        printf("name_ref=%lu name=\"%s\" comps=%u numeric=%u contents=%u start=%u",
            (unsigned long)obj->u.geom_vertex_column.name_object_id,
            obj->u.geom_vertex_column.name,
            (unsigned int)obj->u.geom_vertex_column.num_components,
            (unsigned int)obj->u.geom_vertex_column.numeric_type,
            (unsigned int)obj->u.geom_vertex_column.contents,
            (unsigned int)obj->u.geom_vertex_column.start);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_GEOM_VERTEX_ARRAY_FORMAT) {
        printf("stride=%u total_bytes=%u columns=%u/%u",
            (unsigned int)obj->u.geom_vertex_array_format.stride,
            (unsigned int)obj->u.geom_vertex_array_format.total_bytes,
            (unsigned int)obj->u.geom_vertex_array_format.column_count,
            (unsigned int)obj->u.geom_vertex_array_format.total_column_count);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_GEOM_VERTEX_FORMAT) {
        printf("anim=%u transforms=%u arrays=%u/%u",
            (unsigned int)obj->u.geom_vertex_format.animation_type,
            (unsigned int)obj->u.geom_vertex_format.num_transforms,
            (unsigned int)obj->u.geom_vertex_format.array_count,
            (unsigned int)obj->u.geom_vertex_format.total_array_count);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_GEOM_VERTEX_ARRAY_DATA) {
        printf("array_format=%lu usage=%u bytes=%lu preview=%u",
            (unsigned long)obj->u.geom_vertex_array_data.array_format_object_id,
            (unsigned int)obj->u.geom_vertex_array_data.usage_hint,
            (unsigned long)obj->u.geom_vertex_array_data.data_size,
            (unsigned int)obj->u.geom_vertex_array_data.preview_size);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_GEOM_VERTEX_DATA) {
        printf("name=\"%s\" format=%lu arrays=%u/%u usage=%u",
            obj->u.geom_vertex_data.name,
            (unsigned long)obj->u.geom_vertex_data.format_object_id,
            (unsigned int)obj->u.geom_vertex_data.array_count,
            (unsigned int)obj->u.geom_vertex_data.total_array_count,
            (unsigned int)obj->u.geom_vertex_data.usage_hint);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_GEOM_PRIMITIVE) {
        printf("primitive=%s first=%ld vertices=%ld index_type=%u usage=%u indices=%lu ends=%u/%u trailing=%lu",
            obj->u.geom_primitive.primitive_name,
            (long)obj->u.geom_primitive.first_vertex,
            (long)obj->u.geom_primitive.num_vertices,
            (unsigned int)obj->u.geom_primitive.index_type,
            (unsigned int)obj->u.geom_primitive.usage_hint,
            (unsigned long)obj->u.geom_primitive.vertices_object_id,
            (unsigned int)obj->u.geom_primitive.end_count,
            (unsigned int)obj->u.geom_primitive.total_end_count,
            (unsigned long)obj->u.geom_primitive.trailing_bytes);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_GEOM) {
        printf("vdata=%lu primitives=%u/%u prim_type=%u shade=%u",
            (unsigned long)obj->u.geom.vertex_data_object_id,
            (unsigned int)obj->u.geom.primitive_count,
            (unsigned int)obj->u.geom.total_primitive_count,
            (unsigned int)obj->u.geom.primitive_type,
            (unsigned int)obj->u.geom.shade_model);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_PANDA_NODE) {
        printf("name=\"%s\" tags=%u/%u children=%u/%u stashed=%u/%u",
            obj->u.panda_node.name,
            (unsigned int)obj->u.panda_node.tag_count,
            (unsigned int)obj->u.panda_node.total_tag_count,
            (unsigned int)obj->u.panda_node.child_count,
            (unsigned int)obj->u.panda_node.total_child_count,
            (unsigned int)obj->u.panda_node.stashed_count,
            (unsigned int)obj->u.panda_node.total_stashed_count);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_MODEL_NODE) {
        printf("name=\"%s\" preserve_transform=%u preserve_attributes=%u",
            obj->u.model_node.base.name,
            (unsigned int)obj->u.model_node.preserve_transform,
            (unsigned int)obj->u.model_node.preserve_attributes);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_MODEL_ROOT) {
        printf("name=\"%s\" loader_type=%u:%s",
            obj->u.model_root.base.base.name,
            (unsigned int)obj->u.model_root.loader_type_id,
            obj->u.model_root.loader_type_name);
        return;
    }
    if (obj->class_kind == P3_BAM_CLASS_GEOM_NODE) {
        printf("name=\"%s\" geoms=%u/%u children=%u/%u",
            obj->u.geom_node.base.name,
            (unsigned int)obj->u.geom_node.geom_count,
            (unsigned int)obj->u.geom_node.total_geom_count,
            (unsigned int)obj->u.geom_node.base.child_count,
            (unsigned int)obj->u.geom_node.base.total_child_count);
        return;
    }
    printf("payload=%lu bytes", (unsigned long)0);
}

static void print_scene_summary(const p3_bam_scene_object *obj) {
    if (obj->class_kind == P3_BAM_SCENE_RENDER_STATE) {
        printf("attribs=%u/%u",
            (unsigned int)obj->u.render_state.attrib_count,
            (unsigned int)obj->u.render_state.total_attrib_count);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_TRANSFORM_STATE) {
        printf("flags=%lu repr=%u pos=",
            (unsigned long)obj->u.transform_state.flags,
            (unsigned int)obj->u.transform_state.representation);
        print_fixed_value(obj->u.transform_state.pos[0]);
        printf(",");
        print_fixed_value(obj->u.transform_state.pos[1]);
        printf(",");
        print_fixed_value(obj->u.transform_state.pos[2]);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_TEXTURE_STAGE) {
        printf("name=\"%s\" mode=%u sort=%ld priority=%ld texcoord=%s rgb_combine=%u/%u alpha_combine=%u/%u rgb_pack_src=%u,%u,%u alpha_pack_src=%u,%u,%u flags=color:%u color_scale:%u primary:%u last:%u",
            obj->u.texture_stage.name,
            (unsigned int)obj->u.texture_stage.mode,
            (long)obj->u.texture_stage.sort,
            (long)obj->u.texture_stage.priority,
            obj->u.texture_stage.texcoord_name,
            (unsigned int)obj->u.texture_stage.combine_rgb_mode,
            (unsigned int)obj->u.texture_stage.num_combine_rgb_operands,
            (unsigned int)obj->u.texture_stage.combine_alpha_mode,
            (unsigned int)obj->u.texture_stage.num_combine_alpha_operands,
            (unsigned int)obj->u.texture_stage.combine_rgb_source[0],
            (unsigned int)obj->u.texture_stage.combine_rgb_source[1],
            (unsigned int)obj->u.texture_stage.combine_rgb_source[2],
            (unsigned int)obj->u.texture_stage.combine_alpha_source[0],
            (unsigned int)obj->u.texture_stage.combine_alpha_source[1],
            (unsigned int)obj->u.texture_stage.combine_alpha_source[2],
            (unsigned int)obj->u.texture_stage.uses_color,
            (unsigned int)obj->u.texture_stage.involves_color_scale,
            (unsigned int)obj->u.texture_stage.uses_primary_color,
            (unsigned int)obj->u.texture_stage.uses_last_saved_result);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_TEXTURE) {
        printf("name=\"%s\" file=\"%s\" size=%u x %u raw=%u bytes=%lu",
            obj->u.texture.name,
            obj->u.texture.filename,
            (unsigned int)obj->u.texture.size_x,
            (unsigned int)obj->u.texture.size_y,
            (unsigned int)obj->u.texture.has_rawdata,
            (unsigned long)obj->u.texture.ram_image_size);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_MATERIAL) {
        printf("name=\"%s\" flags=%lu base_color=",
            obj->u.material.name,
            (unsigned long)obj->u.material.flags);
        print_fixed_value(obj->u.material.base_color[0]);
        printf(",");
        print_fixed_value(obj->u.material.base_color[1]);
        printf(",");
        print_fixed_value(obj->u.material.base_color[2]);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_TEXTURE_ATTRIB) {
        printf("off_all=%u off=%u/%u on=%u/%u",
            (unsigned int)obj->u.texture_attrib.off_all_stages,
            (unsigned int)obj->u.texture_attrib.off_stage_count,
            (unsigned int)obj->u.texture_attrib.total_off_stage_count,
            (unsigned int)obj->u.texture_attrib.on_stage_count,
            (unsigned int)obj->u.texture_attrib.total_on_stage_count);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_MATERIAL_ATTRIB) {
        printf("material=%lu", (unsigned long)obj->u.material_attrib.material_object_id);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_TRANSFORM_TABLE) {
        printf("transforms=%u/%u",
            (unsigned int)obj->u.transform_table.transform_count,
            (unsigned int)obj->u.transform_table.total_transform_count);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_TRANSFORM_BLEND) {
        printf("entries=%u/%u",
            (unsigned int)obj->u.transform_blend.entry_count,
            (unsigned int)obj->u.transform_blend.total_entry_count);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_TRANSFORM_BLEND_TABLE) {
        printf("blends=%u/%u rows=%u",
            (unsigned int)obj->u.transform_blend_table.blend_count,
            (unsigned int)obj->u.transform_blend_table.total_blend_count,
            (unsigned int)obj->u.transform_blend_table.rows);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_JOINT_VERTEX_TRANSFORM) {
        printf("joint=%lu", (unsigned long)obj->u.joint_vertex_transform.joint_object_id);
        return;
    }
    if (obj->class_kind == P3_BAM_SCENE_CHARACTER_JOINT) {
        printf("name=\"%s\" children=%u/%u net=%u/%u local=%u/%u",
            obj->u.character_joint.name,
            (unsigned int)obj->u.character_joint.child_count,
            (unsigned int)obj->u.character_joint.total_child_count,
            (unsigned int)obj->u.character_joint.net_node_count,
            (unsigned int)obj->u.character_joint.total_net_node_count,
            (unsigned int)obj->u.character_joint.local_node_count,
            (unsigned int)obj->u.character_joint.total_local_node_count);
        return;
    }
    printf("scene payload");
}

static int export_p3m(const p3_bam_archive *archive, const char *out_path) {
    p3_bam_ir ir;
    int rc;
    rc = p3_bam_extract_ir(archive, &ir);
    if (rc != P3_OK) {
        p3_error_message(rc, err_buf);
        printf("ir error: %s\n", err_buf);
        return 1;
    }
    rc = p3_p3m_write_file(out_path, &ir);
    if (rc != P3_OK) {
        p3_error_message(rc, err_buf);
        printf("export error: %s\n", err_buf);
        return 1;
    }
    printf("ir textures=%u materials=%u joints=%u vertices=%u indices=%u submeshes=%u\n",
        (unsigned int)ir.texture_count,
        (unsigned int)ir.material_count,
        (unsigned int)ir.joint_count,
        (unsigned int)ir.vertex_count,
        (unsigned int)ir.index_count,
        (unsigned int)ir.submesh_count);
    printf("wrote %s\n", out_path);
    return 0;
}

static void print_usage(const char *argv0) {
    printf("usage:\n");
    printf("  %s input.bam [copy.bam]\n", argv0);
    printf("  %s --p3m input.bam output.p3m\n", argv0);
}

int main(int argc, char **argv) {
    p3_u32 size;
    p3_u32 out_size;
    p3_bam_archive archive;
    p3_u16 i;
    int rc;
    int mode_export_p3m;
    const char *input_path;
    const char *copy_path;
    const char *p3m_path;

    mode_export_p3m = 0;
    input_path = 0;
    copy_path = 0;
    p3m_path = 0;

    if (argc >= 2 && strcmp(argv[1], "--p3m") == 0) {
        mode_export_p3m = 1;
        if (argc != 4) {
            print_usage(argv[0]);
            return 1;
        }
        input_path = argv[2];
        p3m_path = argv[3];
    } else {
        if (argc < 2 || argc > 3) {
            print_usage(argv[0]);
            return 1;
        }
        input_path = argv[1];
        if (argc == 3) {
            copy_path = argv[2];
        }
    }

    rc = p3_file_read_all(input_path, file_buf, P3_MAX_FILE_BYTES, &size);
    if (rc != P3_OK) {
        p3_error_message(rc, err_buf);
        printf("read error: %s\n", err_buf);
        return 1;
    }

    rc = p3_bam_read_file(&archive, file_buf, size, err_buf, sizeof(err_buf));
    if (rc != P3_OK) {
        printf("bam error: %s\n", err_buf);
        return 1;
    }

    printf("bam %u.%u endian=%u stdfloat_double=%u types=%u records=%u\n",
        (unsigned int)archive.major,
        (unsigned int)archive.minor,
        (unsigned int)archive.endian,
        (unsigned int)archive.stdfloat_double,
        (unsigned int)archive.type_count,
        (unsigned int)archive.record_count);

    for (i = 0U; i < archive.record_count; ++i) {
        const p3_bam_record *rec;
        p3_bam_object obj;
        p3_bam_scene_object scene_obj;
        rec = &archive.records[i];
        printf("[%u] %s", (unsigned int)i, kind_name(rec->kind));
        if (rec->kind == P3_BAM_BOC_PUSH || rec->kind == P3_BAM_BOC_ADJUNCT) {
            rc = p3_bam_parse_object(&archive, i, &obj);
            if (rc == P3_OK && obj.class_kind != P3_BAM_CLASS_UNKNOWN) {
                printf(" obj=%lu type=%u:%s class=%s ",
                    (unsigned long)rec->object_id,
                    (unsigned int)rec->type_id,
                    obj.type_name,
                    p3_bam_class_name(obj.class_kind));
                print_object_summary(&obj);
            } else {
                rc = p3_bam_scene_parse_object(&archive, i, &scene_obj);
                if (rc == P3_OK) {
                    printf(" obj=%lu type=%u:%s class=%s ",
                        (unsigned long)rec->object_id,
                        (unsigned int)rec->type_id,
                        scene_obj.type_name,
                        p3_bam_scene_class_name(scene_obj.class_kind));
                    print_scene_summary(&scene_obj);
                } else if (rc == P3_OK || obj.class_kind != P3_BAM_CLASS_UNKNOWN) {
                    printf(" obj=%lu type=%u:%s class=%s ",
                        (unsigned long)rec->object_id,
                        (unsigned int)rec->type_id,
                        obj.type_name,
                        p3_bam_class_name(obj.class_kind));
                    print_object_summary(&obj);
                } else {
                    printf(" obj=%lu type=%u payload=%lu bytes",
                        (unsigned long)rec->object_id,
                        (unsigned int)rec->type_id,
                        (unsigned long)rec->payload_len);
                }
            }
        } else if (rec->kind == P3_BAM_BOC_FILE_DATA) {
            printf(" data=%lu bytes", (unsigned long)rec->data_len);
        } else if (rec->kind == P3_BAM_BOC_REMOVE) {
            printf(" count=%u", (unsigned int)rec->remove_count);
        }
        printf("\n");
    }

    if (mode_export_p3m) {
        return export_p3m(&archive, p3m_path);
    }

    if (copy_path != 0) {
        rc = p3_bam_write_file(&archive, out_buf, P3_MAX_FILE_BYTES, &out_size, err_buf, sizeof(err_buf));
        if (rc != P3_OK) {
            printf("write error: %s\n", err_buf);
            return 1;
        }
        rc = p3_file_write_all(copy_path, out_buf, out_size);
        if (rc != P3_OK) {
            p3_error_message(rc, err_buf);
            printf("save error: %s\n", err_buf);
            return 1;
        }
        printf("wrote %s\n", copy_path);
    }

    return 0;
}
