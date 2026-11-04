#include "md3_internal.h"

int md3_read_surface_header(FILE *fp, long base_offset,
    md3_surface_info_t *out_info)
{
    md3_u32 ident;
    md3_u8 name_bytes[MD3_MAX_QPATH];
    int rc;

    if (fp == 0 || out_info == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_io_seek(fp, base_offset);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_io_read_u32_le(fp, &ident);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (ident != MD3_IDENT_U32)
    {
        return MD3_EFORMAT;
    }

    rc = md3_io_read_exact(fp, name_bytes, MD3_MAX_QPATH);
    if (rc != MD3_OK)
    {
        return rc;
    }

    md3_zero_memory(out_info, (unsigned int) sizeof(*out_info));
    md3_read_field_to_cstr(out_info->name, MD3_MAX_QPATH,
        (const char *) name_bytes, MD3_MAX_QPATH);

    rc = md3_io_read_i32_le(fp, &out_info->flags);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_info->num_frames);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_info->num_shaders);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_info->num_verts);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_info->num_triangles);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_info->ofs_triangles);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_info->ofs_shaders);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_info->ofs_st);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_info->ofs_xyznormals);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_info->ofs_end);
    if (rc != MD3_OK) return rc;

    out_info->file_ofs = (md3_i32) base_offset;
    return MD3_OK;
}

int md3_write_surface_header(FILE *fp, const md3_surface_info_t *surface_info)
{
    md3_u8 name_bytes[MD3_MAX_QPATH];

    if (fp == 0 || surface_info == 0)
    {
        return MD3_EINVAL;
    }

    md3_copy_cstr_to_field((char *) name_bytes, MD3_MAX_QPATH, surface_info->name);

    if (md3_io_write_u32_le(fp, MD3_IDENT_U32) != MD3_OK) return MD3_EIO;
    if (md3_io_write_exact(fp, name_bytes, MD3_MAX_QPATH) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->flags) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->num_frames) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->num_shaders) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->num_verts) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->num_triangles) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->ofs_triangles) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->ofs_shaders) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->ofs_st) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->ofs_xyznormals) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, surface_info->ofs_end) != MD3_OK) return MD3_EIO;

    return MD3_OK;
}
