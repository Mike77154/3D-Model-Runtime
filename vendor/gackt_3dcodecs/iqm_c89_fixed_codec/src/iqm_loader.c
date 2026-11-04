#include "iqm_loader.h"

static iqm_u16 iqm_read_le16(const iqm_u8 *p)
{
    return (iqm_u16)((iqm_u16)p[0] | ((iqm_u16)p[1] << 8));
}

static iqm_u32 iqm_read_le32(const iqm_u8 *p)
{
    return ((iqm_u32)p[0]) |
           ((iqm_u32)p[1] << 8) |
           ((iqm_u32)p[2] << 16) |
           ((iqm_u32)p[3] << 24);
}

static iqm_i32 iqm_read_le_i32(const iqm_u8 *p)
{
    return (iqm_i32)iqm_read_le32(p);
}

static int iqm_span_ok(iqm_u32 file_size, iqm_u32 offset, iqm_u32 bytes)
{
    if(offset > file_size) return 0;
    if(bytes > file_size - offset) return 0;
    return 1;
}

static int iqm_count_span_ok(iqm_u32 file_size, iqm_u32 offset, iqm_u32 count, iqm_u32 elem_size)
{
    if(count == 0u) return 1;
    if(offset == 0u) return 0;
    if(elem_size != 0u && count > (iqm_u32)0xffffffffu / elem_size) return 0;
    return iqm_span_ok(file_size, offset, count * elem_size);
}

static int iqm_offset_aligned(iqm_u32 offset)
{
    if(offset == 0u) return 1;
    return (offset & 3u) == 0u;
}

static int iqm_cstr_equal(const char *a, const char *b)
{
    if(a == 0 || b == 0) return 0;
    while(*a != 0 && *b != 0)
    {
        if(*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static int iqm_validate_table(iqm_u32 file_size, iqm_u32 count, iqm_u32 offset, iqm_u32 elem_size)
{
    if(count == 0u) return offset == 0u ? IQM_OK : IQM_ERR_BOUNDS;
    if(!iqm_offset_aligned(offset)) return IQM_ERR_ALIGN;
    if(!iqm_count_span_ok(file_size, offset, count, elem_size)) return IQM_ERR_BOUNDS;
    return IQM_OK;
}

static iqm_u32 iqm_format_bytes(iqm_u32 format)
{
    switch(format)
    {
        case IQM_BYTE: return 1u;
        case IQM_UBYTE: return 1u;
        case IQM_SHORT: return 2u;
        case IQM_USHORT: return 2u;
        case IQM_INT: return 4u;
        case IQM_UINT: return 4u;
        case IQM_HALF: return 2u;
        case IQM_FLOAT: return 4u;
        case IQM_DOUBLE: return 8u;
        default: break;
    }
    return 0u;
}

static int iqm_component_to_fixed(const iqm_u8 *p, iqm_u32 format, iqm_fixed *out_value)
{
    iqm_u32 u;
    iqm_i32 s;

    if(out_value == 0) return IQM_ERR_NULL;

    switch(format)
    {
        case IQM_BYTE:
            s = (iqm_i32)((iqm_i8)p[0]);
            *out_value = iqm_fixed_from_i32(s);
            return IQM_OK;
        case IQM_UBYTE:
            u = (iqm_u32)p[0];
            *out_value = iqm_fixed_from_i32((iqm_i32)u);
            return IQM_OK;
        case IQM_SHORT:
            s = (iqm_i32)((iqm_i16)iqm_read_le16(p));
            *out_value = iqm_fixed_from_i32(s);
            return IQM_OK;
        case IQM_USHORT:
            u = (iqm_u32)iqm_read_le16(p);
            *out_value = iqm_fixed_from_i32((iqm_i32)u);
            return IQM_OK;
        case IQM_INT:
            s = iqm_read_le_i32(p);
            *out_value = iqm_fixed_from_i32(s);
            return IQM_OK;
        case IQM_UINT:
            u = iqm_read_le32(p);
            if(u > 32767u) *out_value = IQM_FIXED_MAX;
            else *out_value = iqm_fixed_from_i32((iqm_i32)u);
            return IQM_OK;
        case IQM_HALF:
            *out_value = iqm_fixed_from_f16_bits(iqm_read_le16(p));
            return IQM_OK;
        case IQM_FLOAT:
            *out_value = iqm_fixed_from_f32_bits(iqm_read_le32(p));
            return IQM_OK;
        case IQM_DOUBLE:
            return IQM_ERR_UNSUPPORTED_FORMAT;
        default: break;
    }

    return IQM_ERR_UNSUPPORTED_FORMAT;
}

static iqm_u8 iqm_component_to_u8(const iqm_u8 *p, iqm_u32 format)
{
    iqm_u32 u;
    iqm_i32 s;

    switch(format)
    {
        case IQM_BYTE:
            s = (iqm_i32)((iqm_i8)p[0]);
            if(s < 0) return (iqm_u8)0;
            if(s > 255) return (iqm_u8)255;
            return (iqm_u8)s;
        case IQM_UBYTE:
            return p[0];
        case IQM_SHORT:
            s = (iqm_i32)((iqm_i16)iqm_read_le16(p));
            if(s < 0) return (iqm_u8)0;
            if(s > 255) return (iqm_u8)255;
            return (iqm_u8)s;
        case IQM_USHORT:
            u = (iqm_u32)iqm_read_le16(p);
            if(u > 255u) return (iqm_u8)255;
            return (iqm_u8)u;
        case IQM_INT:
            s = iqm_read_le_i32(p);
            if(s < 0) return (iqm_u8)0;
            if(s > 255) return (iqm_u8)255;
            return (iqm_u8)s;
        case IQM_UINT:
            u = iqm_read_le32(p);
            if(u > 255u) return (iqm_u8)255;
            return (iqm_u8)u;
        default: break;
    }
    return (iqm_u8)0;
}

static void iqm_read_header(const iqm_u8 *bytes, iqm_header *h)
{
    const iqm_u8 *p;

    p = bytes + 16u;
    h->version = iqm_read_le32(p); p += 4;
    h->filesize = iqm_read_le32(p); p += 4;
    h->flags = iqm_read_le32(p); p += 4;
    h->num_text = iqm_read_le32(p); p += 4;
    h->ofs_text = iqm_read_le32(p); p += 4;
    h->num_meshes = iqm_read_le32(p); p += 4;
    h->ofs_meshes = iqm_read_le32(p); p += 4;
    h->num_vertexarrays = iqm_read_le32(p); p += 4;
    h->num_vertexes = iqm_read_le32(p); p += 4;
    h->ofs_vertexarrays = iqm_read_le32(p); p += 4;
    h->num_triangles = iqm_read_le32(p); p += 4;
    h->ofs_triangles = iqm_read_le32(p); p += 4;
    h->ofs_adjacency = iqm_read_le32(p); p += 4;
    h->num_joints = iqm_read_le32(p); p += 4;
    h->ofs_joints = iqm_read_le32(p); p += 4;
    h->num_poses = iqm_read_le32(p); p += 4;
    h->ofs_poses = iqm_read_le32(p); p += 4;
    h->num_anims = iqm_read_le32(p); p += 4;
    h->ofs_anims = iqm_read_le32(p); p += 4;
    h->num_frames = iqm_read_le32(p); p += 4;
    h->num_framechannels = iqm_read_le32(p); p += 4;
    h->ofs_frames = iqm_read_le32(p); p += 4;
    h->ofs_bounds = iqm_read_le32(p); p += 4;
    h->num_comment = iqm_read_le32(p); p += 4;
    h->ofs_comment = iqm_read_le32(p); p += 4;
    h->num_extensions = iqm_read_le32(p); p += 4;
    h->ofs_extensions = iqm_read_le32(p);
}

void iqm_model_clear(iqm_model *model)
{
    iqm_u32 i;
    iqm_u32 j;

    if(model == 0) return;

    model->bytes = 0;
    model->byte_count = 0u;

    model->header.version = 0u;
    model->header.filesize = 0u;
    model->header.flags = 0u;
    model->header.num_text = 0u;
    model->header.ofs_text = 0u;
    model->header.num_meshes = 0u;
    model->header.ofs_meshes = 0u;
    model->header.num_vertexarrays = 0u;
    model->header.num_vertexes = 0u;
    model->header.ofs_vertexarrays = 0u;
    model->header.num_triangles = 0u;
    model->header.ofs_triangles = 0u;
    model->header.ofs_adjacency = 0u;
    model->header.num_joints = 0u;
    model->header.ofs_joints = 0u;
    model->header.num_poses = 0u;
    model->header.ofs_poses = 0u;
    model->header.num_anims = 0u;
    model->header.ofs_anims = 0u;
    model->header.num_frames = 0u;
    model->header.num_framechannels = 0u;
    model->header.ofs_frames = 0u;
    model->header.ofs_bounds = 0u;
    model->header.num_comment = 0u;
    model->header.ofs_comment = 0u;
    model->header.num_extensions = 0u;
    model->header.ofs_extensions = 0u;

    model->text_offset = 0u;
    model->text_count = 0u;
    model->comment_offset = 0u;
    model->comment_count = 0u;
    model->num_meshes = 0u;
    model->num_vertexes = 0u;
    model->num_triangles = 0u;
    model->num_joints = 0u;
    model->num_poses = 0u;
    model->num_anims = 0u;
    model->num_frames = 0u;
    model->num_framechannels = 0u;
    model->num_framevalues = 0u;
    model->num_bounds = 0u;
    model->num_custom_vertexarrays = 0u;
    model->num_extensions_loaded = 0u;
    model->has_adjacency = 0u;

    model->has_position = 0u;
    model->has_texcoord = 0u;
    model->has_normal = 0u;
    model->has_tangent = 0u;
    model->has_blendindexes = 0u;
    model->has_blendweights = 0u;
    model->has_color = 0u;

    i = 0u;
    while(i < IQM_MAX_VERTEXES)
    {
        j = 0u;
        while(j < 3u)
        {
            model->vertexes[i].position[j] = 0;
            model->vertexes[i].normal[j] = 0;
            j++;
        }
        j = 0u;
        while(j < 2u)
        {
            model->vertexes[i].texcoord[j] = 0;
            j++;
        }
        j = 0u;
        while(j < 4u)
        {
            model->vertexes[i].tangent[j] = 0;
            model->vertexes[i].blendindexes[j] = 0u;
            model->vertexes[i].blendweights[j] = 0u;
            model->vertexes[i].color[j] = 255u;
            j++;
        }
        i++;
    }
}

static int iqm_text_offset_ok(const iqm_model *model, iqm_u32 offset)
{
    const iqm_u8 *text;
    iqm_u32 i;

    if(model->text_count == 0u) return offset == 0u;
    if(offset >= model->text_count) return 0;

    text = model->bytes + model->text_offset;
    i = offset;
    while(i < model->text_count)
    {
        if(text[i] == 0u) return 1;
        i++;
    }
    return 0;
}

static int iqm_validate_text_table(const iqm_model *model)
{
    const iqm_u8 *text;

    if(model == 0) return IQM_ERR_NULL;
    if(model->text_count == 0u) return IQM_OK;
    text = model->bytes + model->text_offset;
    if(text[0] != 0u) return IQM_ERR_BAD_TEXT;
    if(text[model->text_count - 1u] != 0u) return IQM_ERR_BAD_TEXT;
    return IQM_OK;
}

static int iqm_load_meshes(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 i;
    iqm_mesh *m;

    h = &model->header;
    p = model->bytes + h->ofs_meshes;
    i = 0u;
    while(i < h->num_meshes)
    {
        m = &model->meshes[i];
        m->name = iqm_read_le32(p); p += 4;
        m->material = iqm_read_le32(p); p += 4;
        m->first_vertex = iqm_read_le32(p); p += 4;
        m->num_vertexes = iqm_read_le32(p); p += 4;
        m->first_triangle = iqm_read_le32(p); p += 4;
        m->num_triangles = iqm_read_le32(p); p += 4;

        if(!iqm_text_offset_ok(model, m->name)) return IQM_ERR_BAD_TEXT;
        if(!iqm_text_offset_ok(model, m->material)) return IQM_ERR_BAD_TEXT;
        if(m->first_vertex > h->num_vertexes) return IQM_ERR_BOUNDS;
        if(m->num_vertexes > h->num_vertexes - m->first_vertex) return IQM_ERR_BOUNDS;
        if(m->first_triangle > h->num_triangles) return IQM_ERR_BOUNDS;
        if(m->num_triangles > h->num_triangles - m->first_triangle) return IQM_ERR_BOUNDS;
        i++;
    }
    return IQM_OK;
}

static int iqm_store_custom_vertexarray(iqm_model *model, iqm_u32 type, iqm_u32 flags, iqm_u32 format, iqm_u32 size, iqm_u32 offset)
{
    iqm_custom_vertexarray *va;

    if(model == 0) return IQM_ERR_NULL;
    if(model->num_custom_vertexarrays >= IQM_MAX_CUSTOM_VERTEXARRAYS) return IQM_ERR_LIMIT;
    va = &model->custom_vertexarrays[model->num_custom_vertexarrays];
    va->type = type;
    va->name = type - IQM_CUSTOM;
    va->flags = flags;
    va->format = format;
    va->size = size;
    va->offset = offset;
    model->num_custom_vertexarrays++;
    return IQM_OK;
}

static int iqm_read_vertexarray(iqm_model *model, iqm_u32 type, iqm_u32 format, iqm_u32 size, iqm_u32 offset)
{
    const iqm_u8 *base;
    const iqm_u8 *p;
    iqm_u32 bytes;
    iqm_u32 stride;
    iqm_u32 i;
    iqm_u32 c;
    iqm_fixed value;
    int rc;

    bytes = iqm_format_bytes(format);
    if(bytes == 0u) return IQM_ERR_UNSUPPORTED_FORMAT;
    if(format == IQM_DOUBLE) return IQM_ERR_UNSUPPORTED_FORMAT;
    if(size == 0u) return IQM_ERR_BAD_VERTEXARRAY;
    if(size > 16u) return IQM_ERR_BAD_VERTEXARRAY;
    if(bytes != 0u && size > (iqm_u32)0xffffffffu / bytes) return IQM_ERR_BOUNDS;
    stride = bytes * size;
    if(stride != 0u && model->num_vertexes > (iqm_u32)0xffffffffu / stride) return IQM_ERR_BOUNDS;
    if(!iqm_span_ok(model->header.filesize, offset, model->num_vertexes * stride)) return IQM_ERR_BOUNDS;

    base = model->bytes + offset;

    if(type == IQM_POSITION)
    {
        if(size < 3u) return IQM_ERR_BAD_VERTEXARRAY;
        i = 0u;
        while(i < model->num_vertexes)
        {
            c = 0u;
            while(c < 3u)
            {
                p = base + (i * stride) + (c * bytes);
                rc = iqm_component_to_fixed(p, format, &value);
                if(rc != IQM_OK) return rc;
                model->vertexes[i].position[c] = value;
                c++;
            }
            i++;
        }
        model->has_position = 1u;
        return IQM_OK;
    }

    if(type == IQM_TEXCOORD)
    {
        if(size < 2u) return IQM_ERR_BAD_VERTEXARRAY;
        i = 0u;
        while(i < model->num_vertexes)
        {
            c = 0u;
            while(c < 2u)
            {
                p = base + (i * stride) + (c * bytes);
                rc = iqm_component_to_fixed(p, format, &value);
                if(rc != IQM_OK) return rc;
                model->vertexes[i].texcoord[c] = value;
                c++;
            }
            i++;
        }
        model->has_texcoord = 1u;
        return IQM_OK;
    }

    if(type == IQM_NORMAL)
    {
        if(size < 3u) return IQM_ERR_BAD_VERTEXARRAY;
        i = 0u;
        while(i < model->num_vertexes)
        {
            c = 0u;
            while(c < 3u)
            {
                p = base + (i * stride) + (c * bytes);
                rc = iqm_component_to_fixed(p, format, &value);
                if(rc != IQM_OK) return rc;
                model->vertexes[i].normal[c] = value;
                c++;
            }
            i++;
        }
        model->has_normal = 1u;
        return IQM_OK;
    }

    if(type == IQM_TANGENT)
    {
        if(size < 4u) return IQM_ERR_BAD_VERTEXARRAY;
        i = 0u;
        while(i < model->num_vertexes)
        {
            c = 0u;
            while(c < 4u)
            {
                p = base + (i * stride) + (c * bytes);
                rc = iqm_component_to_fixed(p, format, &value);
                if(rc != IQM_OK) return rc;
                model->vertexes[i].tangent[c] = value;
                c++;
            }
            i++;
        }
        model->has_tangent = 1u;
        return IQM_OK;
    }

    if(type == IQM_BLENDINDEXES)
    {
        if(size < 4u) return IQM_ERR_BAD_VERTEXARRAY;
        i = 0u;
        while(i < model->num_vertexes)
        {
            c = 0u;
            while(c < 4u)
            {
                p = base + (i * stride) + (c * bytes);
                model->vertexes[i].blendindexes[c] = iqm_component_to_u8(p, format);
                c++;
            }
            i++;
        }
        model->has_blendindexes = 1u;
        return IQM_OK;
    }

    if(type == IQM_BLENDWEIGHTS)
    {
        if(size < 4u) return IQM_ERR_BAD_VERTEXARRAY;
        i = 0u;
        while(i < model->num_vertexes)
        {
            c = 0u;
            while(c < 4u)
            {
                p = base + (i * stride) + (c * bytes);
                model->vertexes[i].blendweights[c] = iqm_component_to_u8(p, format);
                c++;
            }
            i++;
        }
        model->has_blendweights = 1u;
        return IQM_OK;
    }

    if(type == IQM_COLOR)
    {
        if(size < 4u) return IQM_ERR_BAD_VERTEXARRAY;
        i = 0u;
        while(i < model->num_vertexes)
        {
            c = 0u;
            while(c < 4u)
            {
                p = base + (i * stride) + (c * bytes);
                model->vertexes[i].color[c] = iqm_component_to_u8(p, format);
                c++;
            }
            i++;
        }
        model->has_color = 1u;
        return IQM_OK;
    }

    return IQM_OK;
}

static int iqm_load_vertexarrays(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 i;
    iqm_u32 type;
    iqm_u32 flags;
    iqm_u32 format;
    iqm_u32 size;
    iqm_u32 offset;
    iqm_u32 bytes;
    iqm_u32 prev_type;
    iqm_u8 have_prev;
    int rc;

    h = &model->header;
    p = model->bytes + h->ofs_vertexarrays;
    prev_type = 0u;
    have_prev = 0u;
    i = 0u;
    while(i < h->num_vertexarrays)
    {
        type = iqm_read_le32(p); p += 4;
        flags = iqm_read_le32(p); p += 4;
        format = iqm_read_le32(p); p += 4;
        size = iqm_read_le32(p); p += 4;
        offset = iqm_read_le32(p); p += 4;

        if(have_prev != 0u && type < prev_type) return IQM_ERR_VERTEXARRAY_ORDER;
        have_prev = 1u;
        prev_type = type;

        bytes = iqm_format_bytes(format);
        if(bytes == 0u) return IQM_ERR_UNSUPPORTED_FORMAT;
        if(bytes < 4u)
        {
            if(!iqm_offset_aligned(offset)) return IQM_ERR_ALIGN;
        }
        else
        {
            if((offset & (bytes - 1u)) != 0u) return IQM_ERR_ALIGN;
        }
        if(type >= IQM_CUSTOM)
        {
            if(!iqm_text_offset_ok(model, type - IQM_CUSTOM)) return IQM_ERR_BAD_TEXT;
            rc = iqm_store_custom_vertexarray(model, type, flags, format, size, offset);
            if(rc != IQM_OK) return rc;
        }
        rc = iqm_read_vertexarray(model, type, format, size, offset);
        if(rc != IQM_OK) return rc;
        i++;
    }
    return IQM_OK;
}

static int iqm_load_triangles(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 i;
    iqm_u32 j;

    h = &model->header;
    p = model->bytes + h->ofs_triangles;
    i = 0u;
    while(i < h->num_triangles)
    {
        j = 0u;
        while(j < 3u)
        {
            model->triangles[i].vertex[j] = iqm_read_le32(p);
            if(model->triangles[i].vertex[j] >= h->num_vertexes) return IQM_ERR_BAD_TRIANGLE;
            p += 4;
            j++;
        }
        i++;
    }
    return IQM_OK;
}

static int iqm_load_adjacency(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 i;
    iqm_u32 j;
    iqm_u32 v;

    if(model == 0) return IQM_ERR_NULL;
    h = &model->header;
    if(h->ofs_adjacency == 0u || h->num_triangles == 0u) return IQM_OK;
    p = model->bytes + h->ofs_adjacency;
    i = 0u;
    while(i < h->num_triangles)
    {
        j = 0u;
        while(j < 3u)
        {
            v = iqm_read_le32(p);
            p += 4;
            if(v != (iqm_u32)0xffffffffu && v >= h->num_triangles) return IQM_ERR_BAD_TRIANGLE;
            model->adjacency[i].triangle[j] = v;
            j++;
        }
        i++;
    }
    model->has_adjacency = 1u;
    return IQM_OK;
}

static int iqm_load_extensions(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 offset;
    iqm_u32 count;
    iqm_extension *ext;

    if(model == 0) return IQM_ERR_NULL;
    h = &model->header;
    if(h->num_extensions == 0u) return IQM_OK;
    if(h->ofs_extensions == 0u) return IQM_ERR_BAD_EXTENSION;
    if(h->num_extensions > IQM_MAX_EXTENSIONS) return IQM_ERR_LIMIT;

    offset = h->ofs_extensions;
    count = 0u;
    while(count < h->num_extensions)
    {
        if(offset == 0u) return IQM_ERR_BAD_EXTENSION;
        if(!iqm_offset_aligned(offset)) return IQM_ERR_ALIGN;
        if(!iqm_span_ok(h->filesize, offset, 16u)) return IQM_ERR_BOUNDS;
        p = model->bytes + offset;
        ext = &model->extensions[count];
        ext->name = iqm_read_le32(p); p += 4;
        ext->num_data = iqm_read_le32(p); p += 4;
        ext->ofs_data = iqm_read_le32(p); p += 4;
        ext->ofs_extensions = iqm_read_le32(p);

        if(!iqm_text_offset_ok(model, ext->name)) return IQM_ERR_BAD_TEXT;
        if(ext->num_data != 0u)
        {
            if(ext->ofs_data == 0u) return IQM_ERR_BAD_EXTENSION;
            if(!iqm_span_ok(h->filesize, ext->ofs_data, ext->num_data)) return IQM_ERR_BOUNDS;
        }
        offset = ext->ofs_extensions;
        count++;
    }
    if(offset != 0u) return IQM_ERR_BAD_EXTENSION;
    model->num_extensions_loaded = count;
    return IQM_OK;
}

static int iqm_load_joints(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 i;
    iqm_u32 j;
    iqm_joint *joint;

    h = &model->header;
    p = model->bytes + h->ofs_joints;
    i = 0u;
    while(i < h->num_joints)
    {
        joint = &model->joints[i];
        joint->name = iqm_read_le32(p); p += 4;
        joint->parent = iqm_read_le_i32(p); p += 4;
        if(!iqm_text_offset_ok(model, joint->name)) return IQM_ERR_BAD_TEXT;
        if(joint->parent >= (iqm_i32)i) return IQM_ERR_BOUNDS;

        j = 0u;
        while(j < 3u)
        {
            joint->translate[j] = iqm_fixed_from_f32_bits(iqm_read_le32(p));
            p += 4;
            j++;
        }
        j = 0u;
        while(j < 4u)
        {
            joint->rotate[j] = iqm_fixed_from_f32_bits(iqm_read_le32(p));
            p += 4;
            j++;
        }
        j = 0u;
        while(j < 3u)
        {
            joint->scale[j] = iqm_fixed_from_f32_bits(iqm_read_le32(p));
            p += 4;
            j++;
        }
        i++;
    }
    return IQM_OK;
}

static iqm_u32 iqm_popcount10(iqm_u32 mask)
{
    iqm_u32 c;
    iqm_u32 i;
    c = 0u;
    i = 0u;
    while(i < 10u)
    {
        if((mask & (1u << i)) != 0u) c++;
        i++;
    }
    return c;
}

static int iqm_load_poses(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 i;
    iqm_u32 j;
    iqm_u32 channel_sum;
    iqm_pose *pose;

    h = &model->header;
    p = model->bytes + h->ofs_poses;
    channel_sum = 0u;
    i = 0u;
    while(i < h->num_poses)
    {
        pose = &model->poses[i];
        pose->parent = iqm_read_le_i32(p); p += 4;
        pose->channelmask = iqm_read_le32(p); p += 4;
        pose->channelmask &= 0x3ffu;
        pose->first_channel = channel_sum;
        if(pose->parent >= (iqm_i32)i) return IQM_ERR_BOUNDS;
        channel_sum += iqm_popcount10(pose->channelmask);

        j = 0u;
        while(j < 10u)
        {
            pose->channeloffset[j] = iqm_fixed_from_f32_bits(iqm_read_le32(p));
            p += 4;
            j++;
        }
        j = 0u;
        while(j < 10u)
        {
            pose->channelscale[j] = iqm_fixed_from_f32_bits(iqm_read_le32(p));
            p += 4;
            j++;
        }
        i++;
    }

    if(channel_sum != h->num_framechannels) return IQM_ERR_BAD_FRAMECHANNELS;
    return IQM_OK;
}

static int iqm_load_anims(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 i;
    iqm_anim *anim;

    h = &model->header;
    p = model->bytes + h->ofs_anims;
    i = 0u;
    while(i < h->num_anims)
    {
        anim = &model->anims[i];
        anim->name = iqm_read_le32(p); p += 4;
        anim->first_frame = iqm_read_le32(p); p += 4;
        anim->num_frames = iqm_read_le32(p); p += 4;
        anim->framerate = iqm_fixed_from_f32_bits(iqm_read_le32(p)); p += 4;
        anim->flags = iqm_read_le32(p); p += 4;
        if(!iqm_text_offset_ok(model, anim->name)) return IQM_ERR_BAD_TEXT;
        if(anim->first_frame > h->num_frames) return IQM_ERR_BOUNDS;
        if(anim->num_frames > h->num_frames - anim->first_frame) return IQM_ERR_BOUNDS;
        i++;
    }
    return IQM_OK;
}

static int iqm_load_bounds(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 i;
    iqm_u32 j;

    h = &model->header;
    if(h->num_frames == 0u || h->ofs_bounds == 0u) return IQM_OK;
    p = model->bytes + h->ofs_bounds;
    i = 0u;
    while(i < h->num_frames)
    {
        j = 0u;
        while(j < 3u)
        {
            model->bounds[i].bbmins[j] = iqm_fixed_from_f32_bits(iqm_read_le32(p));
            p += 4;
            j++;
        }
        j = 0u;
        while(j < 3u)
        {
            model->bounds[i].bbmaxs[j] = iqm_fixed_from_f32_bits(iqm_read_le32(p));
            p += 4;
            j++;
        }
        model->bounds[i].xyradius = iqm_fixed_from_f32_bits(iqm_read_le32(p)); p += 4;
        model->bounds[i].radius = iqm_fixed_from_f32_bits(iqm_read_le32(p)); p += 4;
        i++;
    }
    model->num_bounds = h->num_frames;
    return IQM_OK;
}

static int iqm_load_frames(iqm_model *model)
{
    const iqm_header *h;
    const iqm_u8 *p;
    iqm_u32 total;
    iqm_u32 i;

    h = &model->header;
    if(h->num_frames == 0u || h->num_framechannels == 0u) return IQM_OK;
    if(h->num_frames > (iqm_u32)0xffffffffu / h->num_framechannels) return IQM_ERR_BOUNDS;
    total = h->num_frames * h->num_framechannels;
    if(total > IQM_MAX_FRAMEVALUES) return IQM_ERR_LIMIT;
    if(!iqm_span_ok(h->filesize, h->ofs_frames, total * 2u)) return IQM_ERR_BOUNDS;

    p = model->bytes + h->ofs_frames;
    i = 0u;
    while(i < total)
    {
        model->framevalues[i] = iqm_read_le16(p);
        p += 2;
        i++;
    }
    model->num_framevalues = total;
    return IQM_OK;
}

static int iqm_validate_header_tables(const iqm_header *h)
{
    int rc;
    iqm_u32 frame_count_bytes;

    if(h->num_meshes > IQM_MAX_MESHES) return IQM_ERR_LIMIT;
    if(h->num_vertexarrays > IQM_MAX_VERTEXARRAYS) return IQM_ERR_LIMIT;
    if(h->num_vertexes > IQM_MAX_VERTEXES) return IQM_ERR_LIMIT;
    if(h->num_triangles > IQM_MAX_TRIANGLES) return IQM_ERR_LIMIT;
    if(h->num_joints > IQM_MAX_JOINTS) return IQM_ERR_LIMIT;
    if(h->num_poses > IQM_MAX_POSES) return IQM_ERR_LIMIT;
    if(h->num_anims > IQM_MAX_ANIMS) return IQM_ERR_LIMIT;
    if(h->num_frames > IQM_MAX_FRAMES) return IQM_ERR_LIMIT;
    if(h->num_frames > IQM_MAX_BOUNDS) return IQM_ERR_LIMIT;
    if(h->num_extensions > IQM_MAX_EXTENSIONS) return IQM_ERR_LIMIT;
    if(h->num_triangles == 0u && h->ofs_adjacency != 0u) return IQM_ERR_BOUNDS;
    if(h->num_frames == 0u && h->ofs_bounds != 0u) return IQM_ERR_BOUNDS;
    if((h->num_frames == 0u || h->num_framechannels == 0u) && h->ofs_frames != 0u) return IQM_ERR_BOUNDS;
    if(h->num_extensions == 0u && h->ofs_extensions != 0u) return IQM_ERR_BOUNDS;

    rc = iqm_validate_table(h->filesize, h->num_text, h->ofs_text, 1u);
    if(rc != IQM_OK) return rc;
    rc = iqm_validate_table(h->filesize, h->num_meshes, h->ofs_meshes, 24u);
    if(rc != IQM_OK) return rc;
    rc = iqm_validate_table(h->filesize, h->num_vertexarrays, h->ofs_vertexarrays, 20u);
    if(rc != IQM_OK) return rc;
    rc = iqm_validate_table(h->filesize, h->num_triangles, h->ofs_triangles, 12u);
    if(rc != IQM_OK) return rc;
    rc = iqm_validate_table(h->filesize, h->num_joints, h->ofs_joints, 48u);
    if(rc != IQM_OK) return rc;
    rc = iqm_validate_table(h->filesize, h->num_poses, h->ofs_poses, 88u);
    if(rc != IQM_OK) return rc;
    rc = iqm_validate_table(h->filesize, h->num_anims, h->ofs_anims, 20u);
    if(rc != IQM_OK) return rc;
    rc = iqm_validate_table(h->filesize, h->num_comment, h->ofs_comment, 1u);
    if(rc != IQM_OK) return rc;

    if(h->num_frames != 0u && h->ofs_bounds != 0u)
    {
        rc = iqm_validate_table(h->filesize, h->num_frames, h->ofs_bounds, 32u);
        if(rc != IQM_OK) return rc;
    }

    if(h->num_frames != 0u && h->num_framechannels != 0u)
    {
        if(h->num_frames > (iqm_u32)0xffffffffu / h->num_framechannels) return IQM_ERR_BOUNDS;
        if(h->num_frames * h->num_framechannels > (iqm_u32)0xffffffffu / 2u) return IQM_ERR_BOUNDS;
        frame_count_bytes = h->num_frames * h->num_framechannels * 2u;
        if(!iqm_offset_aligned(h->ofs_frames)) return IQM_ERR_ALIGN;
        if(!iqm_span_ok(h->filesize, h->ofs_frames, frame_count_bytes)) return IQM_ERR_BOUNDS;
    }

    if(h->ofs_adjacency != 0u)
    {
        if(!iqm_offset_aligned(h->ofs_adjacency)) return IQM_ERR_ALIGN;
        if(!iqm_span_ok(h->filesize, h->ofs_adjacency, h->num_triangles * 12u)) return IQM_ERR_BOUNDS;
    }

    if(h->num_extensions != 0u && h->ofs_extensions != 0u)
    {
        if(!iqm_offset_aligned(h->ofs_extensions)) return IQM_ERR_ALIGN;
        if(!iqm_span_ok(h->filesize, h->ofs_extensions, 16u)) return IQM_ERR_BOUNDS;
    }

    return IQM_OK;
}

int iqm_load_memory(const iqm_u8 *bytes, iqm_u32 byte_count, iqm_model *out_model)
{
    iqm_header h;
    int rc;
    iqm_u32 i;

    if(bytes == 0 || out_model == 0) return IQM_ERR_NULL;
    iqm_model_clear(out_model);

    if(byte_count < IQM_HEADER_SIZE) return IQM_ERR_TOO_SMALL;
    i = 0u;
    while(i < 15u)
    {
        if(bytes[i] != (iqm_u8)IQM_MAGIC[i]) return IQM_ERR_BAD_MAGIC;
        i++;
    }
    if(bytes[15] != 0u) return IQM_ERR_BAD_MAGIC;

    iqm_read_header(bytes, &h);
    if(h.version != IQM_VERSION) return IQM_ERR_BAD_VERSION;
    if(h.filesize < IQM_HEADER_SIZE) return IQM_ERR_BAD_FILESIZE;
    if(h.filesize > byte_count) return IQM_ERR_BAD_FILESIZE;

    rc = iqm_validate_header_tables(&h);
    if(rc != IQM_OK) return rc;

    out_model->bytes = bytes;
    out_model->byte_count = byte_count;
    out_model->header = h;
    out_model->text_offset = h.ofs_text;
    out_model->text_count = h.num_text;
    out_model->comment_offset = h.ofs_comment;
    out_model->comment_count = h.num_comment;
    out_model->num_meshes = h.num_meshes;
    out_model->num_vertexes = h.num_vertexes;
    out_model->num_triangles = h.num_triangles;
    out_model->num_joints = h.num_joints;
    out_model->num_poses = h.num_poses;
    out_model->num_anims = h.num_anims;
    out_model->num_frames = h.num_frames;
    out_model->num_framechannels = h.num_framechannels;

    rc = iqm_validate_text_table(out_model);
    if(rc != IQM_OK) return rc;

    rc = iqm_load_meshes(out_model);
    if(rc != IQM_OK) return rc;
    rc = iqm_load_vertexarrays(out_model);
    if(rc != IQM_OK) return rc;
    rc = iqm_load_triangles(out_model);
    if(rc != IQM_OK) return rc;
    rc = iqm_load_adjacency(out_model);
    if(rc != IQM_OK) return rc;
    rc = iqm_load_joints(out_model);
    if(rc != IQM_OK) return rc;
    rc = iqm_load_poses(out_model);
    if(rc != IQM_OK) return rc;
    rc = iqm_load_anims(out_model);
    if(rc != IQM_OK) return rc;
    rc = iqm_load_frames(out_model);
    if(rc != IQM_OK) return rc;
    rc = iqm_load_bounds(out_model);
    if(rc != IQM_OK) return rc;
    rc = iqm_load_extensions(out_model);
    if(rc != IQM_OK) return rc;

    return IQM_OK;
}

const char *iqm_get_text(const iqm_model *model, iqm_u32 offset)
{
    if(model == 0) return "";
    if(model->bytes == 0) return "";
    if(model->text_count == 0u) return "";
    if(offset >= model->text_count) return "";
    if(!iqm_text_offset_ok(model, offset)) return "";
    return (const char *)(model->bytes + model->text_offset + offset);
}

int iqm_get_extension_data(const iqm_model *model, const char *name, const iqm_u8 **out_data, iqm_u32 *out_size)
{
    iqm_u32 i;
    const char *ext_name;

    if(out_data != 0) *out_data = 0;
    if(out_size != 0) *out_size = 0u;
    if(model == 0 || name == 0) return IQM_ERR_NULL;
    i = 0u;
    while(i < model->num_extensions_loaded)
    {
        ext_name = iqm_get_text(model, model->extensions[i].name);
        if(iqm_cstr_equal(ext_name, name))
        {
            if(out_data != 0) *out_data = model->bytes + model->extensions[i].ofs_data;
            if(out_size != 0) *out_size = model->extensions[i].num_data;
            return IQM_OK;
        }
        i++;
    }
    return IQM_ERR_BAD_EXTENSION;
}

int iqm_get_pose_channel(const iqm_model *model, iqm_u32 frame_index, iqm_u32 pose_index, iqm_u32 channel_index, iqm_fixed *out_value)
{
    const iqm_pose *pose;
    iqm_u32 local;
    iqm_u32 c;
    iqm_u32 base;
    iqm_u32 idx;
    iqm_u16 raw;
    iqm_fixed scaled;

    if(model == 0 || out_value == 0) return IQM_ERR_NULL;
    if(frame_index >= model->num_frames) return IQM_ERR_BOUNDS;
    if(pose_index >= model->num_poses) return IQM_ERR_BOUNDS;
    if(channel_index >= 10u) return IQM_ERR_BOUNDS;

    pose = &model->poses[pose_index];
    if((pose->channelmask & (1u << channel_index)) == 0u)
    {
        *out_value = pose->channeloffset[channel_index];
        return IQM_OK;
    }

    local = 0u;
    c = 0u;
    while(c < channel_index)
    {
        if((pose->channelmask & (1u << c)) != 0u) local++;
        c++;
    }

    base = frame_index * model->num_framechannels;
    idx = base + pose->first_channel + local;
    if(idx >= model->num_framevalues) return IQM_ERR_BOUNDS;
    raw = model->framevalues[idx];
    scaled = iqm_fixed_mul_u16(pose->channelscale[channel_index], raw);
    *out_value = pose->channeloffset[channel_index] + scaled;
    return IQM_OK;
}

const char *iqm_error_string(int code)
{
    switch(code)
    {
        case IQM_OK: return "ok";
        case IQM_ERR_NULL: return "null argument";
        case IQM_ERR_TOO_SMALL: return "buffer too small";
        case IQM_ERR_BAD_MAGIC: return "bad IQM magic";
        case IQM_ERR_BAD_VERSION: return "unsupported IQM version";
        case IQM_ERR_BAD_FILESIZE: return "bad IQM filesize";
        case IQM_ERR_BOUNDS: return "offset/count out of bounds";
        case IQM_ERR_ALIGN: return "offset alignment error";
        case IQM_ERR_LIMIT: return "model exceeds static limits";
        case IQM_ERR_UNSUPPORTED_FORMAT: return "unsupported vertex component format";
        case IQM_ERR_BAD_VERTEXARRAY: return "bad vertex array";
        case IQM_ERR_BAD_TEXT: return "bad text table offset";
        case IQM_ERR_BAD_TRIANGLE: return "triangle references invalid vertex";
        case IQM_ERR_BAD_FRAMECHANNELS: return "frame channel count does not match pose masks";
        case IQM_ERR_BAD_EXTENSION: return "bad or missing IQM extension";
        case IQM_ERR_VERTEXARRAY_ORDER: return "vertex arrays are not in IQM standard order";
        default: break;
    }
    return "unknown error";
}
