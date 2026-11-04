#include "bravure_bvh_internal.h"

int bbvh_validate(const bbvh_document *doc)
{
    int i;
    int j;
    int total_channels;
    int parent;
    int child;
    int next;
    int channel;
    int ctype;

    if (doc == (const bbvh_document *)0) {
        return BBVH_ERR_NULL;
    }
    if (doc->node_count <= 0 || doc->node_count > BRAVURE_BVH_MAX_NODES) {
        return BBVH_ERR_BAD_DOCUMENT;
    }
    if (doc->root_index < 0 || doc->root_index >= doc->node_count) {
        return BBVH_ERR_BAD_DOCUMENT;
    }
    if (doc->nodes[doc->root_index].parent != -1) {
        return BBVH_ERR_BAD_DOCUMENT;
    }
    if (doc->channel_count < 0 || doc->channel_count > BRAVURE_BVH_MAX_CHANNELS) {
        return BBVH_ERR_BAD_DOCUMENT;
    }
    if (doc->frame_count < 0 || doc->frame_count > BRAVURE_BVH_MAX_FRAMES) {
        return BBVH_ERR_BAD_DOCUMENT;
    }
    if (doc->frame_count > 0 && doc->frame_time < 0L) {
        return BBVH_ERR_BAD_DOCUMENT;
    }

    total_channels = 0;
    for (i = 0; i < doc->node_count; ++i) {
        parent = doc->nodes[i].parent;
        if (i == doc->root_index) {
            if (parent != -1) {
                return BBVH_ERR_BAD_DOCUMENT;
            }
        } else {
            if (parent < 0 || parent >= doc->node_count || parent >= i) {
                return BBVH_ERR_BAD_DOCUMENT;
            }
        }
        child = doc->nodes[i].first_child;
        if (child != -1 && (child < 0 || child >= doc->node_count)) {
            return BBVH_ERR_BAD_DOCUMENT;
        }
        next = doc->nodes[i].next_sibling;
        if (next != -1 && (next < 0 || next >= doc->node_count)) {
            return BBVH_ERR_BAD_DOCUMENT;
        }
        if (doc->nodes[i].depth < 0 || doc->nodes[i].depth >= BRAVURE_BVH_MAX_DEPTH) {
            return BBVH_ERR_BAD_DOCUMENT;
        }
        if (doc->nodes[i].channel_count < 0 ||
            doc->nodes[i].channel_count > BRAVURE_BVH_MAX_NODE_CHANNELS) {
            return BBVH_ERR_BAD_DOCUMENT;
        }
        if (doc->nodes[i].channel_start < 0 ||
            doc->nodes[i].channel_start + doc->nodes[i].channel_count > doc->channel_count) {
            return BBVH_ERR_BAD_DOCUMENT;
        }
        total_channels += doc->nodes[i].channel_count;
        for (j = 0; j < doc->nodes[i].channel_count; ++j) {
            channel = doc->nodes[i].channel_start + j;
            ctype = (int)doc->nodes[i].channel_types[j];
            if (ctype == BBVH_CH_UNKNOWN) {
                return BBVH_ERR_BAD_CHANNEL;
            }
            if (channel < 0 || channel >= BRAVURE_BVH_MAX_CHANNELS) {
                return BBVH_ERR_BAD_DOCUMENT;
            }
            if ((int)doc->channel_types[channel] != ctype) {
                return BBVH_ERR_BAD_DOCUMENT;
            }
            if (doc->channel_nodes[channel] != i) {
                return BBVH_ERR_BAD_DOCUMENT;
            }
        }
    }
    if (total_channels != doc->channel_count) {
        return BBVH_ERR_BAD_DOCUMENT;
    }
    return BBVH_OK;
}

int bbvh_count_end_sites(const bbvh_document *doc)
{
    int i;
    int count;

    if (doc == (const bbvh_document *)0) {
        return 0;
    }
    count = 0;
    for (i = 0; i < doc->node_count; ++i) {
        if (doc->nodes[i].has_end_site) {
            count += 1;
        }
    }
    return count;
}

bbvh_fixed bbvh_get_duration(const bbvh_document *doc)
{
    int i;
    bbvh_fixed d;

    if (doc == (const bbvh_document *)0 || doc->frame_count <= 0) {
        return 0L;
    }
    d = 0L;
    for (i = 0; i < doc->frame_count; ++i) {
        d = bbvh_fixed_add_sat(d, doc->frame_time);
    }
    return d;
}

int bbvh_get_info(const bbvh_document *doc, bbvh_info *out_info)
{
    int i;
    int max_depth;
    int rc;

    if (doc == (const bbvh_document *)0 || out_info == (bbvh_info *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    max_depth = 0;
    for (i = 0; i < doc->node_count; ++i) {
        if (doc->nodes[i].depth > max_depth) {
            max_depth = doc->nodes[i].depth;
        }
    }
    out_info->node_count = doc->node_count;
    out_info->channel_count = doc->channel_count;
    out_info->frame_count = doc->frame_count;
    out_info->end_site_count = bbvh_count_end_sites(doc);
    out_info->max_depth = max_depth;
    out_info->frame_time = doc->frame_time;
    out_info->duration = bbvh_get_duration(doc);
    return BBVH_OK;
}

int bbvh_find_node(const bbvh_document *doc, const char *name)
{
    int i;

    if (doc == (const bbvh_document *)0 || name == (const char *)0) {
        return -1;
    }
    for (i = 0; i < doc->node_count; ++i) {
        if (bbvh_str_eq(doc->nodes[i].name, name)) {
            return i;
        }
    }
    for (i = 0; i < doc->node_count; ++i) {
        if (bbvh_str_ieq(doc->nodes[i].name, name)) {
            return i;
        }
    }
    return -1;
}
