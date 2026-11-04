#include "ir_scene_stream.h"

#include <string.h>

#define IR_STREAM_ATTR_POOL_CAP (IR_XML_MAX_ATTRS * (IR_XML_NAME_CAP + IR_XML_VALUE_CAP + 16) + 256)

static void ir_scene_stream_seterr(char* err, size_t err_cap, const char* msg)
{
    size_t n;
    if (!err || err_cap == 0U) {
        return;
    }
    if (!msg) {
        msg = "error";
    }
    n = strlen(msg);
    if (n >= err_cap) {
        n = err_cap - 1U;
    }
    memcpy(err, msg, n);
    err[n] = '\0';
}

typedef struct ir_scene_stream_attr_pack_tag {
    ir_attr attr;
    ir_strpool pool;
    char pool_mem[IR_STREAM_ATTR_POOL_CAP];
} ir_scene_stream_attr_pack;

static int ir_scene_stream_make_attr_pack(const ir_xml_tag* tag, ir_scene_stream_attr_pack* pack, char* err, size_t err_cap)
{
    if (!pack) {
        return IR_ERR_ARGUMENT;
    }
    ir_strpool_init(&pack->pool, pack->pool_mem, sizeof(pack->pool_mem));
    return ir_attr_from_xml_tag(tag, &pack->attr, &pack->pool, err, err_cap);
}

static int ir_scene_stream_emit_root_attr(const ir_scene_stream_events* events, void* user, const ir_attr* attr, const ir_strpool* pool, char* err, size_t err_cap)
{
    int rc;
    if (!events) {
        return IR_OK;
    }
    if (events->on_root_attr) {
        rc = events->on_root_attr(user, attr);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    if (events->on_root_attr_ex) {
        rc = events->on_root_attr_ex(user, attr, pool);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    return IR_OK;
}

static int ir_scene_stream_emit_root_user_attr(const ir_scene_stream_events* events, void* user, const ir_attr* attr, const ir_strpool* pool, char* err, size_t err_cap)
{
    int rc;
    if (!events) {
        return IR_OK;
    }
    if (events->on_root_user_attr) {
        rc = events->on_root_user_attr(user, attr);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    if (events->on_root_user_attr_ex) {
        rc = events->on_root_user_attr_ex(user, attr, pool);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    return IR_OK;
}

static int ir_scene_stream_emit_node_attr(const ir_scene_stream_events* events, void* user, ir_u32 depth, const char* type, const ir_attr* attr, const ir_strpool* pool, char* err, size_t err_cap)
{
    int rc;
    if (!events) {
        return IR_OK;
    }
    if (events->on_node_attr) {
        rc = events->on_node_attr(user, depth, type, attr);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    if (events->on_node_attr_ex) {
        rc = events->on_node_attr_ex(user, depth, type, attr, pool);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    return IR_OK;
}

static int ir_scene_stream_emit_node_user_attr(const ir_scene_stream_events* events, void* user, ir_u32 depth, const char* type, const ir_attr* attr, const ir_strpool* pool, char* err, size_t err_cap)
{
    int rc;
    if (!events) {
        return IR_OK;
    }
    if (events->on_node_user_attr) {
        rc = events->on_node_user_attr(user, depth, type, attr);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    if (events->on_node_user_attr_ex) {
        rc = events->on_node_user_attr_ex(user, depth, type, attr, pool);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    return IR_OK;
}

static int ir_scene_stream_parse_attributes(ir_xml_reader* xr, char* err, size_t err_cap,
    int (*emit_attr)(const ir_scene_stream_events*, void*, const ir_attr*, const ir_strpool*, char*, size_t),
    const ir_scene_stream_events* events, void* user)
{
    ir_xml_tag tag;
    int rc;
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "attributes")) {
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (emit_attr) {
            ir_scene_stream_attr_pack pack;
            rc = ir_scene_stream_make_attr_pack(&tag, &pack, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
            rc = emit_attr(events, user, &pack.attr, &pack.pool, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
        if (!tag.is_empty) {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

struct ir_scene_node_attr_ctx {
    const ir_scene_stream_events* events;
    void* user;
    ir_u32 depth;
    const char* type;
    ir_u32 index;
};

static int ir_scene_stream_emit_node_material_begin_ctx(const struct ir_scene_node_attr_ctx* ctx, char* err, size_t err_cap)
{
    int rc;
    if (!ctx->events || !ctx->events->on_node_material_begin) {
        return IR_OK;
    }
    rc = ctx->events->on_node_material_begin(ctx->user, ctx->depth, ctx->type, ctx->index);
    if (rc != IR_OK) {
        ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
    }
    return rc;
}

static int ir_scene_stream_emit_node_material_end_ctx(const struct ir_scene_node_attr_ctx* ctx, char* err, size_t err_cap)
{
    int rc;
    if (!ctx->events || !ctx->events->on_node_material_end) {
        return IR_OK;
    }
    rc = ctx->events->on_node_material_end(ctx->user, ctx->depth, ctx->type, ctx->index);
    if (rc != IR_OK) {
        ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
    }
    return rc;
}

static int ir_scene_stream_emit_node_material_attr_ctx(const struct ir_scene_node_attr_ctx* ctx, const ir_attr* attr, const ir_strpool* pool, char* err, size_t err_cap)
{
    int rc;
    if (!ctx->events) {
        return IR_OK;
    }
    if (ctx->events->on_node_material_attr) {
        rc = ctx->events->on_node_material_attr(ctx->user, ctx->depth, ctx->type, ctx->index, attr);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    if (ctx->events->on_node_material_attr_ex) {
        rc = ctx->events->on_node_material_attr_ex(ctx->user, ctx->depth, ctx->type, ctx->index, attr, pool);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    return IR_OK;
}

static int ir_scene_stream_emit_node_animator_begin_ctx(const struct ir_scene_node_attr_ctx* ctx, char* err, size_t err_cap)
{
    int rc;
    if (!ctx->events || !ctx->events->on_node_animator_begin) {
        return IR_OK;
    }
    rc = ctx->events->on_node_animator_begin(ctx->user, ctx->depth, ctx->type, ctx->index);
    if (rc != IR_OK) {
        ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
    }
    return rc;
}

static int ir_scene_stream_emit_node_animator_end_ctx(const struct ir_scene_node_attr_ctx* ctx, char* err, size_t err_cap)
{
    int rc;
    if (!ctx->events || !ctx->events->on_node_animator_end) {
        return IR_OK;
    }
    rc = ctx->events->on_node_animator_end(ctx->user, ctx->depth, ctx->type, ctx->index);
    if (rc != IR_OK) {
        ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
    }
    return rc;
}

static int ir_scene_stream_emit_node_animator_attr_ctx(const struct ir_scene_node_attr_ctx* ctx, const ir_attr* attr, const ir_strpool* pool, char* err, size_t err_cap)
{
    int rc;
    if (!ctx->events) {
        return IR_OK;
    }
    if (ctx->events->on_node_animator_attr) {
        rc = ctx->events->on_node_animator_attr(ctx->user, ctx->depth, ctx->type, ctx->index, attr);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    if (ctx->events->on_node_animator_attr_ex) {
        rc = ctx->events->on_node_animator_attr_ex(ctx->user, ctx->depth, ctx->type, ctx->index, attr, pool);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    return IR_OK;
}

static int ir_scene_stream_parse_attributes_node(ir_xml_reader* xr, char* err, size_t err_cap, const struct ir_scene_node_attr_ctx* ctx, int is_material)
{
    ir_xml_tag tag;
    int rc;
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "attributes")) {
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        {
            ir_scene_stream_attr_pack pack;
            rc = ir_scene_stream_make_attr_pack(&tag, &pack, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
            if (is_material) {
                rc = ir_scene_stream_emit_node_material_attr_ctx(ctx, &pack.attr, &pack.pool, err, err_cap);
            } else {
                rc = ir_scene_stream_emit_node_animator_attr_ctx(ctx, &pack.attr, &pack.pool, err, err_cap);
            }
            if (rc != IR_OK) {
                return rc;
            }
        }
        if (!tag.is_empty) {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

static int ir_scene_stream_parse_userdata_node(ir_xml_reader* xr, char* err, size_t err_cap, const ir_scene_stream_events* events, void* user, ir_u32 depth, const char* type)
{
    ir_xml_tag tag;
    int rc;
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "userData")) {
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            if (tag.is_empty) {
                continue;
            }
            for (;;) {
                rc = ir_xml_next_tag(xr, &tag, err, err_cap);
                if (rc != IR_OK) {
                    return rc;
                }
                if (tag.is_end) {
                    if (!ir_xml_name_eq(tag.name, "attributes")) {
                        return IR_ERR_FORMAT;
                    }
                    break;
                }
                {
                    ir_scene_stream_attr_pack pack;
                    rc = ir_scene_stream_make_attr_pack(&tag, &pack, err, err_cap);
                    if (rc != IR_OK) {
                        return rc;
                    }
                    rc = ir_scene_stream_emit_node_user_attr(events, user, depth, type, &pack.attr, &pack.pool, err, err_cap);
                    if (rc != IR_OK) {
                        return rc;
                    }
                }
                if (!tag.is_empty) {
                    rc = ir_xml_skip_element(xr, &tag, err, err_cap);
                    if (rc != IR_OK) {
                        return rc;
                    }
                }
            }
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

static int ir_scene_stream_parse_materials(ir_xml_reader* xr, char* err, size_t err_cap, const ir_scene_stream_events* events, void* user, ir_u32 depth, const char* type)
{
    ir_xml_tag tag;
    int rc;
    ir_u32 index;
    index = 0UL;
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "materials")) {
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            struct ir_scene_node_attr_ctx ctx;
            ctx.events = events;
            ctx.user = user;
            ctx.depth = depth;
            ctx.type = type;
            ctx.index = index;
            rc = ir_scene_stream_emit_node_material_begin_ctx(&ctx, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_attributes_node(xr, err, err_cap, &ctx, 1);
                if (rc != IR_OK) {
                    return rc;
                }
            }
            rc = ir_scene_stream_emit_node_material_end_ctx(&ctx, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
            index += 1UL;
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

static int ir_scene_stream_parse_animators(ir_xml_reader* xr, char* err, size_t err_cap, const ir_scene_stream_events* events, void* user, ir_u32 depth, const char* type)
{
    ir_xml_tag tag;
    int rc;
    ir_u32 index;
    index = 0UL;
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "animators")) {
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            struct ir_scene_node_attr_ctx ctx;
            ctx.events = events;
            ctx.user = user;
            ctx.depth = depth;
            ctx.type = type;
            ctx.index = index;
            rc = ir_scene_stream_emit_node_animator_begin_ctx(&ctx, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_attributes_node(xr, err, err_cap, &ctx, 0);
                if (rc != IR_OK) {
                    return rc;
                }
            }
            rc = ir_scene_stream_emit_node_animator_end_ctx(&ctx, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
            index += 1UL;
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

static int ir_scene_stream_parse_node(ir_xml_reader* xr, const ir_xml_tag* start_tag, char* err, size_t err_cap, const ir_scene_stream_events* events, void* user, ir_u32 depth);
static int ir_scene_stream_parse_scene_container(ir_xml_reader* xr, const ir_xml_tag* start_tag, char* err, size_t err_cap, const ir_scene_stream_events* events, void* user, int has_context, ir_u32 depth, const char* type);

static int ir_scene_stream_parse_root_userdata(ir_xml_reader* xr, char* err, size_t err_cap, const ir_scene_stream_events* events, void* user)
{
    ir_xml_tag tag;
    int rc;
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "userData")) {
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_attributes(xr, err, err_cap, ir_scene_stream_emit_root_user_attr, events, user);
                if (rc != IR_OK) {
                    return rc;
                }
            }
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

static int ir_scene_stream_parse_scene_container(ir_xml_reader* xr, const ir_xml_tag* start_tag, char* err, size_t err_cap, const ir_scene_stream_events* events, void* user, int has_context, ir_u32 depth, const char* type)
{
    ir_xml_tag tag;
    int rc;
    if (!xr || !start_tag) {
        return IR_ERR_ARGUMENT;
    }
    if (!type) {
        type = "";
    }
    if (start_tag->is_empty) {
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "irr_scene")) {
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            if (tag.is_empty) {
                continue;
            }
            if (has_context) {
                for (;;) {
                    rc = ir_xml_next_tag(xr, &tag, err, err_cap);
                    if (rc != IR_OK) {
                        return rc;
                    }
                    if (tag.is_end) {
                        if (!ir_xml_name_eq(tag.name, "attributes")) {
                            return IR_ERR_FORMAT;
                        }
                        break;
                    }
                    {
                        ir_scene_stream_attr_pack pack;
                        rc = ir_scene_stream_make_attr_pack(&tag, &pack, err, err_cap);
                        if (rc != IR_OK) {
                            return rc;
                        }
                        rc = ir_scene_stream_emit_node_attr(events, user, depth, type, &pack.attr, &pack.pool, err, err_cap);
                        if (rc != IR_OK) {
                            return rc;
                        }
                    }
                    if (!tag.is_empty) {
                        rc = ir_xml_skip_element(xr, &tag, err, err_cap);
                        if (rc != IR_OK) {
                            return rc;
                        }
                    }
                }
            } else {
                rc = ir_scene_stream_parse_attributes(xr, err, err_cap, ir_scene_stream_emit_root_attr, events, user);
                if (rc != IR_OK) {
                    return rc;
                }
            }
        } else if (ir_xml_name_eq(tag.name, "userData")) {
            if (tag.is_empty) {
                continue;
            }
            if (has_context) {
                rc = ir_scene_stream_parse_userdata_node(xr, err, err_cap, events, user, depth, type);
            } else {
                rc = ir_scene_stream_parse_root_userdata(xr, err, err_cap, events, user);
            }
            if (rc != IR_OK) {
                return rc;
            }
        } else if (has_context && ir_xml_name_eq(tag.name, "materials")) {
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_materials(xr, err, err_cap, events, user, depth, type);
                if (rc != IR_OK) {
                    return rc;
                }
            }
        } else if (has_context && ir_xml_name_eq(tag.name, "animators")) {
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_animators(xr, err, err_cap, events, user, depth, type);
                if (rc != IR_OK) {
                    return rc;
                }
            }
        } else if (ir_xml_name_eq(tag.name, "node")) {
            rc = ir_scene_stream_parse_node(xr, &tag, err, err_cap, events, user, has_context ? depth + 1U : depth);
            if (rc != IR_OK) {
                return rc;
            }
        } else if (ir_xml_name_eq(tag.name, "irr_scene")) {
            rc = ir_scene_stream_parse_scene_container(xr, &tag, err, err_cap, events, user, has_context, depth, type);
            if (rc != IR_OK) {
                return rc;
            }
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

static int ir_scene_stream_parse_node(ir_xml_reader* xr, const ir_xml_tag* start_tag, char* err, size_t err_cap, const ir_scene_stream_events* events, void* user, ir_u32 depth)
{
    ir_xml_tag tag;
    int rc;
    const char* type;
    type = ir_xml_tag_attr_local(start_tag, "type");
    if (!type) {
        type = "";
    }
    if (events && events->on_node_begin) {
        rc = events->on_node_begin(user, depth, type, start_tag->line, start_tag->col);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    if (start_tag->is_empty) {
        if (events && events->on_node_end) {
            rc = events->on_node_end(user, depth, type);
            if (rc != IR_OK) {
                ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
                return rc;
            }
        }
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "node")) {
                return IR_ERR_FORMAT;
            }
            if (events && events->on_node_end) {
                rc = events->on_node_end(user, depth, type);
                if (rc != IR_OK) {
                    ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
                    return rc;
                }
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            if (tag.is_empty) {
                continue;
            }
            for (;;) {
                rc = ir_xml_next_tag(xr, &tag, err, err_cap);
                if (rc != IR_OK) {
                    return rc;
                }
                if (tag.is_end) {
                    if (!ir_xml_name_eq(tag.name, "attributes")) {
                        return IR_ERR_FORMAT;
                    }
                    break;
                }
                {
                    ir_scene_stream_attr_pack pack;
                    rc = ir_scene_stream_make_attr_pack(&tag, &pack, err, err_cap);
                    if (rc != IR_OK) {
                        return rc;
                    }
                    rc = ir_scene_stream_emit_node_attr(events, user, depth, type, &pack.attr, &pack.pool, err, err_cap);
                    if (rc != IR_OK) {
                        return rc;
                    }
                }
                if (!tag.is_empty) {
                    rc = ir_xml_skip_element(xr, &tag, err, err_cap);
                    if (rc != IR_OK) {
                        return rc;
                    }
                }
            }
        } else if (ir_xml_name_eq(tag.name, "materials")) {
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_materials(xr, err, err_cap, events, user, depth, type);
                if (rc != IR_OK) {
                    return rc;
                }
            }
        } else if (ir_xml_name_eq(tag.name, "animators")) {
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_animators(xr, err, err_cap, events, user, depth, type);
                if (rc != IR_OK) {
                    return rc;
                }
            }
        } else if (ir_xml_name_eq(tag.name, "userData")) {
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_userdata_node(xr, err, err_cap, events, user, depth, type);
                if (rc != IR_OK) {
                    return rc;
                }
            }
        } else if (ir_xml_name_eq(tag.name, "node")) {
            rc = ir_scene_stream_parse_node(xr, &tag, err, err_cap, events, user, depth + 1U);
            if (rc != IR_OK) {
                return rc;
            }
        } else if (ir_xml_name_eq(tag.name, "irr_scene")) {
            rc = ir_scene_stream_parse_scene_container(xr, &tag, err, err_cap, events, user, 1, depth, type);
            if (rc != IR_OK) {
                return rc;
            }
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

int ir_scene_parse_memory_stream(const void* data, size_t size, const ir_scene_stream_events* events, void* user, char* err, size_t err_cap)
{
    ir_xml_reader xr;
    ir_xml_tag tag;
    int rc;
    if (!data || size == 0U) {
        return IR_ERR_ARGUMENT;
    }
    ir_xml_reader_init(&xr, data, size);
    for (;;) {
        rc = ir_xml_next_tag(&xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return (rc == IR_EOF) ? IR_ERR_FORMAT : rc;
        }
        if (!tag.is_end && ir_xml_name_eq(tag.name, "irr_scene")) {
            break;
        }
        if (!tag.is_end) {
            rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
    if (events && events->on_scene_begin) {
        rc = events->on_scene_begin(user, tag.line, tag.col);
        if (rc != IR_OK) {
            ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
            return rc;
        }
    }
    if (tag.is_empty) {
        if (events && events->on_scene_end) {
            rc = events->on_scene_end(user);
            if (rc != IR_OK) {
                ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
                return rc;
            }
        }
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(&xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "irr_scene")) {
                return IR_ERR_FORMAT;
            }
            if (events && events->on_scene_end) {
                rc = events->on_scene_end(user);
                if (rc != IR_OK) {
                    ir_scene_stream_seterr(err, err_cap, "scene callback aborted");
                    return rc;
                }
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_attributes(&xr, err, err_cap, ir_scene_stream_emit_root_attr, events, user);
                if (rc != IR_OK) {
                    return rc;
                }
            }
        } else if (ir_xml_name_eq(tag.name, "userData")) {
            if (!tag.is_empty) {
                rc = ir_scene_stream_parse_root_userdata(&xr, err, err_cap, events, user);
                if (rc != IR_OK) {
                    return rc;
                }
            }
        } else if (ir_xml_name_eq(tag.name, "node")) {
            rc = ir_scene_stream_parse_node(&xr, &tag, err, err_cap, events, user, 0U);
            if (rc != IR_OK) {
                return rc;
            }
        } else if (ir_xml_name_eq(tag.name, "irr_scene")) {
            rc = ir_scene_stream_parse_scene_container(&xr, &tag, err, err_cap, events, user, 0, 0U, "");
            if (rc != IR_OK) {
                return rc;
            }
        } else {
            rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}
