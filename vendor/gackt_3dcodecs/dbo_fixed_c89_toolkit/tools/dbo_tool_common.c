
#include <stdio.h>
#include <string.h>
#include "dbo_tool_common.h"

static void dbo__tool_copy_err(char* err, dbo_u32 err_size, const char* text)
{
    if (err == 0 || err_size == 0UL)
    {
        return;
    }
    if (text == 0)
    {
        err[0] = '\0';
        return;
    }
    strncpy(err, text, err_size - 1UL);
    err[err_size - 1UL] = '\0';
}

int dbo_tool_read_file(const char* path, void* buffer, dbo_u32 capacity, dbo_u32* out_size, char* err, dbo_u32 err_size)
{
    FILE* f;
    long length;
    size_t got;

    if (out_size != 0)
    {
        *out_size = 0UL;
    }
    if (path == 0 || buffer == 0 || out_size == 0)
    {
        dbo__tool_copy_err(err, err_size, "bad arguments");
        return 0;
    }

    f = fopen(path, "rb");
    if (f == 0)
    {
        dbo__tool_copy_err(err, err_size, "could not open input file");
        return 0;
    }

    if (fseek(f, 0L, SEEK_END) != 0)
    {
        fclose(f);
        dbo__tool_copy_err(err, err_size, "could not seek input file");
        return 0;
    }
    length = ftell(f);
    if (length < 0L)
    {
        fclose(f);
        dbo__tool_copy_err(err, err_size, "could not size input file");
        return 0;
    }
    if ((dbo_u32)length > capacity)
    {
        fclose(f);
        dbo__tool_copy_err(err, err_size, "input file is larger than tool buffer");
        return 0;
    }
    if (fseek(f, 0L, SEEK_SET) != 0)
    {
        fclose(f);
        dbo__tool_copy_err(err, err_size, "could not rewind input file");
        return 0;
    }

    got = fread(buffer, 1U, (size_t)length, f);
    fclose(f);

    if (got != (size_t)length)
    {
        dbo__tool_copy_err(err, err_size, "could not read full input file");
        return 0;
    }

    *out_size = (dbo_u32)length;
    dbo__tool_copy_err(err, err_size, "");
    return 1;
}

int dbo_tool_write_file(const char* path, const void* buffer, dbo_u32 size, char* err, dbo_u32 err_size)
{
    FILE* f;
    size_t wrote;

    if (path == 0 || buffer == 0)
    {
        dbo__tool_copy_err(err, err_size, "bad arguments");
        return 0;
    }

    f = fopen(path, "wb");
    if (f == 0)
    {
        dbo__tool_copy_err(err, err_size, "could not open output file");
        return 0;
    }

    wrote = fwrite(buffer, 1U, (size_t)size, f);
    fclose(f);

    if (wrote != (size_t)size)
    {
        dbo__tool_copy_err(err, err_size, "could not write full output file");
        return 0;
    }

    dbo__tool_copy_err(err, err_size, "");
    return 1;
}

int dbo_tool_load_scene(const char* path, dbo_scene* scene, void* buffer, dbo_u32 capacity, dbo_u32* out_size, char* err, dbo_u32 err_size)
{
    if (!dbo_tool_read_file(path, buffer, capacity, out_size, err, err_size))
    {
        return 0;
    }
    if (!dbo_parse_memory(scene, buffer, *out_size))
    {
        dbo__tool_copy_err(err, err_size, dbo_scene_last_error(scene));
        return 0;
    }
    dbo__tool_copy_err(err, err_size, "");
    return 1;
}

void dbo_tool_fx_to_string(dbo_fx32 value, char* buffer, dbo_u32 buffer_size)
{
    dbo_u32 abs_v;
    dbo_u32 whole;
    dbo_u32 frac;
    const char* sign;

    if (buffer == 0 || buffer_size == 0UL)
    {
        return;
    }

    sign = "";
    if (value < 0)
    {
        sign = "-";
        abs_v = (dbo_u32)(-(value + 1)) + 1UL;
    }
    else
    {
        abs_v = (dbo_u32)value;
    }

    whole = abs_v >> DBO_FX_SHIFT;
    frac = ((abs_v & 0xFFFFUL) * 10000UL) / 65536UL;
    (void)buffer_size;
    sprintf(buffer, "%s%lu.%04lu", sign, (unsigned long)whole, (unsigned long)frac);
}

void dbo_tool_json_write_string(FILE* f, const char* text)
{
    const unsigned char* p;
    if (f == 0)
    {
        return;
    }
    fputc('"', f);
    if (text != 0)
    {
        for (p = (const unsigned char*)text; *p != 0U; ++p)
        {
            switch (*p)
            {
            case '\\':
                fputs("\\\\", f);
                break;
            case '"':
                fputs("\\\"", f);
                break;
            case '\n':
                fputs("\\n", f);
                break;
            case '\r':
                fputs("\\r", f);
                break;
            case '\t':
                fputs("\\t", f);
                break;
            default:
                if (*p < 32U)
                {
                    fprintf(f, "\\u%04x", (unsigned int)*p);
                }
                else
                {
                    fputc((int)*p, f);
                }
                break;
            }
        }
    }
    fputc('"', f);
}

const char* dbo_tool_cstr_or_empty(const dbo_scene* scene, dbo_blob_ref ref)
{
    const char* s = dbo_blob_cstr(scene, ref);
    return s != 0 ? s : "";
}

void dbo_tool_write_fx_json(FILE* f, dbo_fx32 value)
{
    char buf[32];
    dbo_tool_fx_to_string(value, buf, (dbo_u32)sizeof(buf));
    fputs(buf, f);
}

const char* dbo_tool_scope_name(dbo_u32 scope_kind)
{
    switch (scope_kind)
    {
    case DBO_SCOPE_SCENE: return "scene";
    case DBO_SCOPE_FRAME: return "frame";
    case DBO_SCOPE_MESH: return "mesh";
    case DBO_SCOPE_TEXTURE: return "texture";
    case DBO_SCOPE_BONE: return "bone";
    case DBO_SCOPE_MULTIMATERIAL: return "multimaterial";
    case DBO_SCOPE_ANIMATION: return "animation";
    case DBO_SCOPE_ANIMATION_SET: return "animation_set";
    default: return "unknown_scope";
    }
}
