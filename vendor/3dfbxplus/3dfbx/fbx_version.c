#include "fbx_version.h"
#include "fbx_importer.h"

#include <string.h>

static const char *fbx_prop_as_cstr_local(const FbxProperty *p)
{
    if (!p) return 0;
    if (p->type_code != 'S') return 0;
    return (const char*)p->data;
}

static int fbx_prop_is_i64_like_local(const FbxProperty *p)
{
    if (!p) return 0;
    return (p->type_code == 'L' || p->type_code == 'I');
}

static FbxNode *fbx_find_child_local(FbxNode *node, const char *name)
{
    int i;
    if (!node || !name) return 0;
    for (i = 0; i < node->child_count; ++i) {
        FbxNode *ch = node->children[i];
        if (!ch || !ch->name) continue;
        if (strcmp(ch->name, name) == 0) return ch;
    }
    return 0;
}

static int fbx_name_starts_with_local(const char *s, const char *prefix)
{
    size_t n;
    if (!s || !prefix) return 0;
    n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static int fbx_is_embedded_mesh_model_local(FbxNode *model)
{
    const char *type_str;
    if (!model || !model->name) return 0;
    if (strcmp(model->name, "Model") != 0) return 0;
    if (model->property_count < 2) return 0;

    /* FBX 6000 legacy Model usually looks like:
         Model: "Mesh1", "Mesh"
       Modern FBX usually looks like:
         Model: id, "Model::Name", "Mesh"
    */
    if (model->properties[0].type_code != 'S') return 0;
    if (model->properties[1].type_code != 'S') return 0;

    type_str = fbx_prop_as_cstr_local(&model->properties[1]);
    if (!type_str || strcmp(type_str, "Mesh") != 0) return 0;

    if (!fbx_find_child_local(model, "Vertices")) return 0;
    if (!fbx_find_child_local(model, "PolygonVertexIndex")) return 0;

    return 1;
}

static int fbx_is_modern_numeric_model_local(FbxNode *model)
{
    const char *type_str;
    if (!model || !model->name) return 0;
    if (strcmp(model->name, "Model") != 0) return 0;
    if (model->property_count < 3) return 0;
    if (!fbx_prop_is_i64_like_local(&model->properties[0])) return 0;
    type_str = fbx_prop_as_cstr_local(&model->properties[2]);
    if (!type_str) return 0;
    if (strcmp(type_str, "Mesh") == 0 ||
        strcmp(type_str, "Camera") == 0 ||
        strcmp(type_str, "Light") == 0 ||
        strcmp(type_str, "LimbNode") == 0 ||
        strcmp(type_str, "Root") == 0) {
        return 1;
    }
    return 0;
}

static int fbx_connect_uses_string_endpoints_local(FbxNode *connect)
{
    if (!connect || !connect->name) return 0;
    if (strcmp(connect->name, "Connect") != 0 && strcmp(connect->name, "C") != 0) return 0;
    if (connect->property_count < 3) return 0;

    /* Old FBX 6000 connections commonly use string endpoints. Modern FBX7+
       usually uses IDs after the relation string. */
    if (connect->properties[1].type_code == 'S' || connect->properties[2].type_code == 'S') {
        return 1;
    }
    return 0;
}

int fbx_get_version_info(FbxContext *ctx, FbxVersionInfo *out) {
    fbx_i32 raw;

    if (!ctx || !out) return 0;

    raw = (fbx_i32)ctx->version;

    out->version_raw = raw;
    /* FBX file version is typically a 4-digit integer like 7400, 7700.
       Common convention (as used in Autodesk headers) is:
         M m r b  (digits)
       Where M = major, m = minor, r = revision, b = build.
       Most public exporters use *00 for the last two digits.
       We keep it simple and split the digits defensively.
    */
    out->major    = raw / 1000;
    out->minor    = (raw / 100) % 10;
    out->revision = (raw / 10) % 10;
    out->year_approx = 0;

    if (raw == 0) {
        out->year_approx = 0;
        return 1;
    }

    /* year_approx is best-effort and only used for UI/debug.
       Autodesk's public docs tend to speak in "FBX 2011/2012/…" terms,
       while the on-disk header uses 4-digit integers.
       The major format jump relevant to binary parsing is 7500 (FBX 7.5).
    */
    if (raw < 4000) {
        out->year_approx = 2000;
    } else if (raw < 5000) {
        out->year_approx = 2002;
    } else if (raw < 6000) {
        out->year_approx = 2003;
    } else if (raw < 7000) {
        /* FBX 6.0/6.x: 2005/2006-era exporters are common here. */
        out->year_approx = 2005;
    } else if (raw < 7100) {
        out->year_approx = 2010;
    } else if (raw < 7200) {
        out->year_approx = 2011;
    } else if (raw < 7300) {
        out->year_approx = 2012;
    } else if (raw < 7400) {
        out->year_approx = 2013;
    } else if (raw < 7500) {
        out->year_approx = 2014;
    } else if (raw < 7600) {
        out->year_approx = 2016;
    } else if (raw < 7700) {
        out->year_approx = 2018;
    } else if (raw < 7800) {
        out->year_approx = 2019;
    } else {
        out->year_approx = 2020;
    }

    return 1;
}

const char *fbx_version_get_profile(const FbxVersionInfo *info) {
    fbx_i32 raw;

    if (!info) return "fbx_unknown";

    raw = info->version_raw;

    if (raw == 0)           return "fbx_unknown";
    if (raw < 4000)         return "fbx_very_legacy";
    if (raw < 5000)         return "fbx_4000";
    if (raw < 6000)         return "fbx_5000";
    if (raw == 6000)        return "fbx_6000_gateable";
    if (raw < 7000)         return "fbx_6x_legacy";
    if (raw < 7100)         return "fbx_70";
    if (raw < 7200)         return "fbx_71";
    if (raw < 7300)         return "fbx_72";
    if (raw < 7400)         return "fbx_73";
    if (raw < 7500)         return "fbx_74";
    if (raw < 7600)         return "fbx_75";
    if (raw < 7700)         return "fbx_76";
    if (raw < 7800)         return "fbx_77";
    return "fbx_75_plus";
}

int fbx_is_version_supported(const FbxVersionInfo *info) {
    fbx_i32 raw;

    if (!info) return 0;

    raw = info->version_raw;

    if (raw == 0) {
        return 1;
    }

    /* "Supported" here means: the loader will *attempt* to parse.
       Practically, binary parsing depends mainly on the 7500 boundary
       (32-bit vs 64-bit node record fields). Everything else is handled
       leniently by name-based parsing.
    */
    if (raw < 0) return 0;
    if (raw == 0) return 1;

    /* Include legacy 4000/5000 in case such files use the same binary magic. */
    if (raw >= 4000 && raw < 10000) return 1;

    return 0;
}

int fbx_version_analyze_scene(FbxNode *root,
                              const FbxVersionInfo *info,
                              FbxVersionSceneProfile *out)
{
    FbxNode *objects;
    FbxNode *connections;
    FbxNode *takes;
    int i;

    if (!root || !info || !out) return 0;
    memset(out, 0, sizeof(*out));
    out->info = *info;
    out->import_path = FBX_IMPORT_PATH_MODERN;

    if (info->version_raw == 6000) {
        out->flags |= FBX_VERSION_FLAG_IS_6000;
    }

    objects = fbx_find_child_local(root, "Objects");
    if (objects) {
        out->object_count = objects->child_count;
        for (i = 0; i < objects->child_count; ++i) {
            FbxNode *n;
            const char *nm;
            const char *ty;
            n = objects->children[i];
            if (!n || !n->name) continue;

            if (strcmp(n->name, "Geometry") == 0) {
                out->modern_geometry_count++;
            }

            if (strcmp(n->name, "Model") == 0) {
                out->model_count++;
                if (fbx_is_modern_numeric_model_local(n)) {
                    out->numeric_model_id_count++;
                } else {
                    out->no_numeric_model_id_count++;
                }

                nm = 0;
                ty = 0;
                if (n->property_count > 0) nm = fbx_prop_as_cstr_local(&n->properties[0]);
                if (n->property_count > 1) ty = fbx_prop_as_cstr_local(&n->properties[1]);
                if (nm && ty &&
                    (strcmp(ty, "Root") == 0 || strcmp(ty, "Null") == 0 || strcmp(ty, "LimbNode") == 0) &&
                    fbx_name_starts_with_local(nm, "Object_")) {
                    out->aux_root_hint_count++;
                }

                if (fbx_is_embedded_mesh_model_local(n)) {
                    out->embedded_model_mesh_count++;
                }
            }
        }
    }

    connections = fbx_find_child_local(root, "Connections");
    if (connections) {
        for (i = 0; i < connections->child_count; ++i) {
            if (fbx_connect_uses_string_endpoints_local(connections->children[i])) {
                out->string_connection_count++;
            }
        }
    }

    takes = fbx_find_child_local(root, "Takes");
    if (takes) {
        for (i = 0; i < takes->child_count; ++i) {
            FbxNode *t;
            const char *take_name;
            t = takes->children[i];
            if (!t || !t->name) continue;
            if (strcmp(t->name, "Take") != 0) continue;
            take_name = 0;
            if (t->property_count > 0) take_name = fbx_prop_as_cstr_local(&t->properties[0]);
            if (take_name && take_name[0] != '\0' && strcmp(take_name, "Current") != 0) {
                out->legacy_take_count++;
            }
        }
    }

    if (out->embedded_model_mesh_count > 0) {
        out->flags |= FBX_VERSION_FLAG_MODEL_EMBEDDED_MESH;
    }
    if (out->modern_geometry_count == 0) {
        out->flags |= FBX_VERSION_FLAG_NO_MODERN_GEOMETRY;
    }
    if (out->embedded_model_mesh_count > 0 && out->numeric_model_id_count == 0) {
        out->flags |= FBX_VERSION_FLAG_MODEL_MESH_NO_NUMERIC_ID;
    }
    if (out->aux_root_hint_count >= 8) {
        out->flags |= FBX_VERSION_FLAG_MANY_AUX_ROOTS;
    }
    if (out->legacy_take_count > 0) {
        out->flags |= FBX_VERSION_FLAG_LEGACY_TAKES;
    }
    if (out->string_connection_count > 0) {
        out->flags |= FBX_VERSION_FLAG_STRING_CONNECTIONS;
    }

    /* Hyper-specific gate:
       Do NOT route every FBX 6000 embedded-Model file into the special importer.
       A clean Noesis-style FBX 6000 can still be handled by a normal/core raw
       path. The legacy adapter is reserved for the "mamon" shape:
         - FBX 6000
         - mesh embedded directly in Model
         - no modern Geometry objects
         - no modern numeric Model IDs
         - PLUS scene-noise symptoms: many Object_n roots or old named Takes.

       string_connections is diagnostic only because both clean and noisy FBX 6000
       files tend to have old string Connect nodes. */
    if ((out->flags & FBX_VERSION_FLAG_IS_6000) &&
        (out->flags & FBX_VERSION_FLAG_MODEL_EMBEDDED_MESH) &&
        (out->flags & FBX_VERSION_FLAG_NO_MODERN_GEOMETRY) &&
        (out->flags & FBX_VERSION_FLAG_MODEL_MESH_NO_NUMERIC_ID) &&
        ((out->flags & FBX_VERSION_FLAG_MANY_AUX_ROOTS) ||
         (out->flags & FBX_VERSION_FLAG_LEGACY_TAKES))) {
        out->flags |= FBX_VERSION_FLAG_HYPER_LEGACY6000_CASE;
        if (out->flags & FBX_VERSION_FLAG_LEGACY_TAKES) {
            out->import_path = FBX_IMPORT_PATH_LEGACY6000_EMBEDDED_MODEL_WITH_TAKES;
        } else {
            out->import_path = FBX_IMPORT_PATH_LEGACY6000_EMBEDDED_MODEL;
        }
    } else {
        out->import_path = FBX_IMPORT_PATH_MODERN;
    }

    return 1;
}

int fbx_version_should_use_legacy6000(const FbxVersionSceneProfile *profile)
{
    if (!profile) return 0;
    return (profile->flags & FBX_VERSION_FLAG_HYPER_LEGACY6000_CASE) ? 1 : 0;
}

const char *fbx_import_path_name(FbxImportPath path)
{
    switch (path) {
        case FBX_IMPORT_PATH_MODERN:
            return "modern";
        case FBX_IMPORT_PATH_LEGACY6000_EMBEDDED_MODEL:
            return "legacy6000_embedded_model";
        case FBX_IMPORT_PATH_LEGACY6000_EMBEDDED_MODEL_WITH_TAKES:
            return "legacy6000_embedded_model_with_takes";
        case FBX_IMPORT_PATH_UNKNOWN:
        default:
            return "unknown";
    }
}
