#include "xps_internal.h"

static int xps_bin_has_tangent(xps_u16 major, xps_u16 minor, int has_header)
{
    if (!has_header) {
        return 1;
    }
    return (major <= 2U && minor <= 12U);
}

static int xps_bin_has_variable_weights(xps_u16 major, int has_header)
{
    if (!has_header) {
        return 0;
    }
    return (major >= 3U);
}

static int xps_bin_skip_header(FILE *fp, xps_model_info *model)
{
    xps_u32 magic;
    xps_header_info header;
    int rc;

    rc = xps_internal_read_u32le(fp, &magic);
    if (rc != XPS_OK) {
        return rc;
    }
    if (magic != XPS_MAGIC_NUMBER) {
        if (fseek(fp, 0L, SEEK_SET) != 0) {
            return XPS_ERR_IO;
        }
        model->format = XPS_FMT_MESH_BIN;
        model->has_header = 0;
        model->version_major = 0;
        model->version_minor = 0;
        return XPS_OK;
    }

    rc = xps_internal_parse_xps_header(fp, NULL, NULL, &header);
    if (rc != XPS_OK) {
        return rc;
    }

    model->format = XPS_FMT_XPS;
    model->has_header = 1;
    model->version_major = header.version_major;
    model->version_minor = header.version_minor;
    return XPS_OK;
}

int xps_internal_parse_bin(FILE *fp,
                           const xps_sink *sink,
                           void *user,
                           xps_model_info *model)
{
    xps_bone bone;
    xps_mesh_info mesh;
    xps_texture tex;
    xps_vertex vertex;
    xps_face face;
    xps_u32 mesh_index;
    xps_u32 bone_index;
    int rc;
    int has_tangent;
    int has_variable_weights;

    if (fp == NULL || model == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        return XPS_ERR_IO;
    }

    rc = xps_bin_skip_header(fp, model);
    if (rc != XPS_OK) {
        return rc;
    }

    has_tangent = xps_bin_has_tangent(model->version_major, model->version_minor, model->has_header);
    has_variable_weights = xps_bin_has_variable_weights(model->version_major, model->has_header);

    rc = xps_internal_read_u32le(fp, &model->bone_count);
    if (rc != XPS_OK) {
        return rc;
    }
    for (bone_index = 0U; bone_index < model->bone_count; ++bone_index) {
        memset(&bone, 0, sizeof(bone));
        bone.index = bone_index;

        rc = xps_internal_read_lpstring(fp, bone.name, sizeof(bone.name));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_i16le(fp, &bone.parent);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &bone.x);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &bone.y);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &bone.z);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_invoke_bone(sink, user, &bone);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    rc = xps_internal_read_u32le(fp, &model->mesh_count);
    if (rc != XPS_OK) {
        return rc;
    }

    for (mesh_index = 0U; mesh_index < model->mesh_count; ++mesh_index) {
        xps_u32 tex_index;
        xps_u32 vertex_index;
        xps_u32 face_index;

        memset(&mesh, 0, sizeof(mesh));
        mesh.index = mesh_index;

        rc = xps_internal_read_lpstring(fp, mesh.name, sizeof(mesh.name));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_u32le(fp, &mesh.uv_layers);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_u32le(fp, &mesh.texture_count);
        if (rc != XPS_OK) {
            return rc;
        }

        rc = xps_internal_invoke_mesh_begin(sink, user, &mesh);
        if (rc != XPS_OK) {
            return rc;
        }

        for (tex_index = 0U; tex_index < mesh.texture_count; ++tex_index) {
            memset(&tex, 0, sizeof(tex));
            tex.index = tex_index;

            rc = xps_internal_read_lpstring(fp, tex.file, sizeof(tex.file));
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u32le(fp, &tex.uv_layer);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_invoke_texture(sink, user, mesh.index, &tex);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        rc = xps_internal_read_u32le(fp, &mesh.vertex_count);
        if (rc != XPS_OK) {
            return rc;
        }

        for (vertex_index = 0U; vertex_index < mesh.vertex_count; ++vertex_index) {
            xps_u32 uv_index;
            xps_i16 weight_count;
            xps_i16 weight_index;

            memset(&vertex, 0, sizeof(vertex));
            vertex.index = vertex_index;

            rc = xps_internal_read_f32fx(fp, &vertex.x);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &vertex.y);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &vertex.z);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &vertex.nx);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &vertex.ny);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &vertex.nz);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &vertex.rgba[0]);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &vertex.rgba[1]);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &vertex.rgba[2]);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &vertex.rgba[3]);
            if (rc != XPS_OK) {
                return rc;
            }

            vertex.uv_count = mesh.uv_layers;
            if (mesh.uv_layers > XPS_CFG_MAX_UV_LAYERS && !XPS_CFG_TRUNCATE_EXTRA_UV) {
                return XPS_ERR_LIMIT;
            }
            for (uv_index = 0U; uv_index < mesh.uv_layers; ++uv_index) {
                xps_fx u;
                xps_fx v;

                rc = xps_internal_read_f32fx(fp, &u);
                if (rc != XPS_OK) {
                    return rc;
                }
                rc = xps_internal_read_f32fx(fp, &v);
                if (rc != XPS_OK) {
                    return rc;
                }
                if (uv_index < XPS_CFG_MAX_UV_LAYERS) {
                    vertex.uv[uv_index][0] = u;
                    vertex.uv[uv_index][1] = v;
                }
                if (has_tangent) {
                    rc = xps_internal_skip(fp, 16U);
                    if (rc != XPS_OK) {
                        return rc;
                    }
                }
            }
            if (vertex.uv_count > XPS_CFG_MAX_UV_LAYERS) {
                vertex.uv_count = XPS_CFG_MAX_UV_LAYERS;
            }

            if (model->bone_count > 0U) {
                if (has_variable_weights) {
                    rc = xps_internal_read_i16le(fp, &weight_count);
                    if (rc != XPS_OK) {
                        return rc;
                    }
                } else {
                    weight_count = 4;
                }
                if (weight_count < 0) {
                    return XPS_ERR_PARSE;
                }
                vertex.weight_count = (xps_u8)weight_count;
                if ((xps_u32)vertex.weight_count > XPS_CFG_MAX_WEIGHTS) {
                    if (!XPS_CFG_TRUNCATE_EXTRA_WEIGHTS) {
                        return XPS_ERR_LIMIT;
                    }
                    vertex.weight_count = XPS_CFG_MAX_WEIGHTS;
                }
                for (weight_index = 0; weight_index < weight_count; ++weight_index) {
                    xps_i16 idv;

                    rc = xps_internal_read_i16le(fp, &idv);
                    if (rc != XPS_OK) {
                        return rc;
                    }
                    if (weight_index < (xps_i16)XPS_CFG_MAX_WEIGHTS) {
                        vertex.weights[weight_index].bone_id = idv;
                    }
                }
                for (weight_index = 0; weight_index < weight_count; ++weight_index) {
                    xps_fx w;

                    rc = xps_internal_read_f32fx(fp, &w);
                    if (rc != XPS_OK) {
                        return rc;
                    }
                    if (weight_index < (xps_i16)XPS_CFG_MAX_WEIGHTS) {
                        vertex.weights[weight_index].value = w;
                    }
                }
            }

            rc = xps_internal_invoke_vertex(sink, user, mesh.index, &vertex);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        rc = xps_internal_read_u32le(fp, &mesh.face_count);
        if (rc != XPS_OK) {
            return rc;
        }
        for (face_index = 0U; face_index < mesh.face_count; ++face_index) {
            memset(&face, 0, sizeof(face));
            face.index = face_index;
            rc = xps_internal_read_u32le(fp, &face.i0);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u32le(fp, &face.i1);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u32le(fp, &face.i2);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_invoke_face(sink, user, mesh.index, &face);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        rc = xps_internal_invoke_mesh_end(sink, user, &mesh);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    return XPS_OK;
}
