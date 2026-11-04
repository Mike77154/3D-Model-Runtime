#include "xps_internal.h"

static int xps_header_has_tangent(xps_u16 major, xps_u16 minor)
{
    return (major <= 2U && minor <= 12U);
}

static void xps_header_trim(char *line)
{
    size_t len;
    size_t start;
    size_t i;

    len = strlen(line);
    while (len > 0U) {
        char c;

        c = line[len - 1U];
        if (c == '\n' || c == '\r' || isspace((unsigned char)c)) {
            line[len - 1U] = '\0';
            --len;
        } else {
            break;
        }
    }

    start = 0U;
    while (line[start] != '\0' && isspace((unsigned char)line[start])) {
        ++start;
    }
    if (start > 0U) {
        for (i = 0U; line[start + i] != '\0'; ++i) {
            line[i] = line[start + i];
        }
        line[i] = '\0';
    }
}

static char *xps_header_next_token(char **cursor)
{
    char *p;
    char *start;

    p = *cursor;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p == '\0') {
        *cursor = p;
        return NULL;
    }
    start = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p != '\0') {
        *p = '\0';
        ++p;
    }
    *cursor = p;
    return start;
}

static int xps_header_parse_pose_line(char *line, xps_u32 index, xps_pose_bone *bone)
{
    char *colon;
    char *cursor;
    char *tok;
    xps_fx values[9];
    int count;
    int rc;

    memset(bone, 0, sizeof(*bone));
    bone->index = index;
    bone->sx = xps_fx_from_int(1);
    bone->sy = xps_fx_from_int(1);
    bone->sz = xps_fx_from_int(1);

    colon = strchr(line, ':');
    if (colon == NULL) {
        return XPS_ERR_PARSE;
    }
    *colon = '\0';
    xps_header_trim(line);
    xps_header_trim(colon + 1);
    if (line[0] == '\0' || (colon + 1)[0] == '\0') {
        return XPS_ERR_PARSE;
    }
    xps_internal_copy_capped(bone->name, sizeof(bone->name), line);

    cursor = colon + 1;
    count = 0;
    for (;;) {
        tok = xps_header_next_token(&cursor);
        if (tok == NULL) {
            break;
        }
        if (count >= 9) {
            return XPS_ERR_PARSE;
        }
        rc = xps_internal_fx_from_ascii(tok, &values[count]);
        if (rc != XPS_OK) {
            return rc;
        }
        ++count;
    }

    if (count != 6 && count != 9) {
        return XPS_ERR_PARSE;
    }

    bone->rx = values[0];
    bone->ry = values[1];
    bone->rz = values[2];
    bone->tx = values[3];
    bone->ty = values[4];
    bone->tz = values[5];
    if (count == 9) {
        bone->sx = values[6];
        bone->sy = values[7];
        bone->sz = values[8];
        bone->has_explicit_scale = 1U;
    }
    return XPS_OK;
}

static int xps_header_process_pose_line(char *line,
                                        xps_u32 *parsed_count,
                                        xps_u32 declared_bones,
                                        const xps_header_sink *sink,
                                        void *user)
{
    xps_pose_bone bone;
    int rc;

    xps_header_trim(line);
    if (line[0] == '\0' || line[0] == '#') {
        return XPS_OK;
    }
    if (declared_bones != 0U && *parsed_count >= declared_bones) {
        return XPS_OK;
    }

    rc = xps_header_parse_pose_line(line, *parsed_count, &bone);
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_invoke_header_default_pose_bone(sink, user, &bone);
    if (rc != XPS_OK) {
        return rc;
    }
    *parsed_count += 1U;
    return XPS_OK;
}

static int xps_header_parse_default_pose(FILE *fp,
                                         xps_u32 payload_bytes,
                                         xps_u32 declared_bones,
                                         const xps_header_sink *sink,
                                         void *user,
                                         xps_header_info *header)
{
    char line[XPS_CFG_MAX_LINE];
    size_t line_len;
    xps_u32 i;
    xps_u32 parsed_count;
    xps_u32 pad;
    xps_header_pose_info pose;
    int rc;

    line[0] = '\0';
    line_len = 0U;
    parsed_count = 0U;

    pose.declared_bone_count = declared_bones;
    pose.parsed_bone_count = 0U;
    pose.byte_count = payload_bytes;

    header->default_pose_declared_bones = declared_bones;
    header->default_pose_bytes = payload_bytes;

    rc = xps_internal_invoke_header_default_pose_begin(sink, user, &pose);
    if (rc != XPS_OK) {
        return rc;
    }

    for (i = 0U; i < payload_bytes; ++i) {
        xps_u8 ch;

        rc = xps_internal_read_u8(fp, &ch);
        if (rc != XPS_OK) {
            return rc;
        }

        if (ch == (xps_u8)'\r' || ch == (xps_u8)'\n') {
            rc = xps_header_process_pose_line(line, &parsed_count, declared_bones, sink, user);
            if (rc != XPS_OK) {
                return rc;
            }
            line[0] = '\0';
            line_len = 0U;
            continue;
        }

        if (line_len + 1U >= sizeof(line)) {
            return XPS_ERR_LINE_TOO_LONG;
        }
        line[line_len] = (char)ch;
        ++line_len;
        line[line_len] = '\0';
    }

    rc = xps_header_process_pose_line(line, &parsed_count, declared_bones, sink, user);
    if (rc != XPS_OK) {
        return rc;
    }

    pad = payload_bytes & 3U;
    if (pad != 0U) {
        rc = xps_internal_skip(fp, 4U - pad);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    header->default_pose_parsed_bones = parsed_count;
    pose.parsed_bone_count = parsed_count;
    return xps_internal_invoke_header_default_pose_end(sink, user, &pose);
}

static void xps_header_apply_flag(xps_header_flags *flags,
                                  xps_u32 flag_id,
                                  xps_u32 raw_value)
{
    switch (flag_id) {
    case 0U:
        flags->back_face_culling = (raw_value != 0U) ? 1U : 0U;
        break;
    case 1U:
        flags->always_force_culling = (raw_value != 0U) ? 1U : 0U;
        break;
    case 2U:
        flags->model_cast_shadows = (raw_value != 0U) ? 1U : 0U;
        break;
    case 3U:
        flags->tangent_space_red = (xps_u8)(raw_value & 1U);
        break;
    case 4U:
        flags->tangent_space_green = (xps_u8)(raw_value & 1U);
        break;
    case 5U:
        flags->tangent_space_blue = (xps_u8)(raw_value & 1U);
        break;
    case 6U:
        flags->gloss_raw = raw_value;
        flags->gloss_is_nonzero = (raw_value != 0U) ? 1U : 0U;
        break;
    case 7U:
        flags->has_bone_directions = (raw_value != 0U) ? 1U : 0U;
        break;
    default:
        flags->unknown_pair_count += 1U;
        break;
    }
}

static int xps_header_parse_flags(FILE *fp,
                                  xps_u32 pair_count,
                                  const xps_header_sink *sink,
                                  void *user,
                                  xps_header_flags *flags,
                                  xps_header_info *header)
{
    xps_u32 i;
    int rc;

    flags->present = 1U;
    flags->pair_count += pair_count;
    header->flags_pair_count += pair_count;

    for (i = 0U; i < pair_count; ++i) {
        xps_u32 flag_id;
        xps_u32 raw_value;
        xps_header_flag_pair pair;

        rc = xps_internal_read_u32le(fp, &flag_id);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_u32le(fp, &raw_value);
        if (rc != XPS_OK) {
            return rc;
        }

        pair.index = i;
        pair.flag_id = flag_id;
        pair.raw_value = raw_value;

        rc = xps_internal_invoke_header_flag_pair(sink, user, &pair);
        if (rc != XPS_OK) {
            return rc;
        }
        xps_header_apply_flag(flags, flag_id, raw_value);
    }
    return XPS_OK;
}

int xps_internal_parse_xps_header(FILE *fp,
                                  const xps_header_sink *sink,
                                  void *user,
                                  xps_header_info *header)
{
    xps_header_info local_header;
    xps_header_flags flags;
    xps_u32 magic;
    xps_u16 major;
    xps_u16 minor;
    xps_u32 settings_len_words;
    xps_u32 settings_len_bytes;
    int rc;
    int has_tangent;

    if (fp == NULL || header == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        return XPS_ERR_IO;
    }

    xps_internal_zero_header_info(&local_header);
    xps_internal_header_flags_default(&flags);

    rc = xps_internal_read_u32le(fp, &magic);
    if (rc != XPS_OK) {
        return rc;
    }
    if (magic != XPS_MAGIC_NUMBER) {
        return XPS_ERR_FORMAT;
    }
    local_header.magic_number = magic;

    rc = xps_internal_read_u16le(fp, &major);
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_u16le(fp, &minor);
    if (rc != XPS_OK) {
        return rc;
    }
    local_header.version_major = major;
    local_header.version_minor = minor;

    rc = xps_internal_read_lpstring(fp, local_header.xna_aral, sizeof(local_header.xna_aral));
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_u32le(fp, &settings_len_words);
    if (rc != XPS_OK) {
        return rc;
    }
    local_header.settings_len_words = settings_len_words;
    settings_len_bytes = settings_len_words * 4U;
    local_header.settings_len_bytes = settings_len_bytes;
    local_header.has_settings = (settings_len_bytes != 0U) ? 1U : 0U;

    rc = xps_internal_read_lpstring(fp, local_header.machine, sizeof(local_header.machine));
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_lpstring(fp, local_header.user, sizeof(local_header.user));
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_lpstring(fp, local_header.files, sizeof(local_header.files));
    if (rc != XPS_OK) {
        return rc;
    }

    if (settings_len_bytes == 0U) {
        local_header.settings_kind = XPS_HEADER_SETTINGS_NONE;
    } else {
        has_tangent = xps_header_has_tangent(major, minor);
        local_header.settings_kind = has_tangent ? XPS_HEADER_SETTINGS_LEGACY_RAW : XPS_HEADER_SETTINGS_TYPED;
    }

    rc = xps_internal_invoke_header_begin(sink, user, &local_header);
    if (rc != XPS_OK) {
        return rc;
    }

    if (local_header.settings_kind == XPS_HEADER_SETTINGS_LEGACY_RAW) {
        rc = xps_internal_skip(fp, settings_len_bytes);
        if (rc != XPS_OK) {
            return rc;
        }
        *header = local_header;
        return xps_internal_invoke_header_end(sink, user, header);
    }

    if (local_header.settings_kind == XPS_HEADER_SETTINGS_TYPED) {
        xps_u32 bytes_read;
        xps_u32 item_index;

        rc = xps_internal_read_u32le(fp, &local_header.settings_hash);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_u32le(fp, &local_header.settings_item_count);
        if (rc != XPS_OK) {
            return rc;
        }
        bytes_read = 8U;

        for (item_index = 0U; item_index < local_header.settings_item_count; ++item_index) {
            xps_header_option option;
            xps_u32 remaining_bytes;

            if (bytes_read >= settings_len_bytes) {
                break;
            }
            if (settings_len_bytes - bytes_read < 12U) {
                return XPS_ERR_PARSE;
            }

            memset(&option, 0, sizeof(option));
            option.index = item_index;

            rc = xps_internal_read_u32le(fp, &option.type_raw);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u32le(fp, &option.count);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u32le(fp, &option.info);
            if (rc != XPS_OK) {
                return rc;
            }
            bytes_read += 12U;

            if (option.type_raw == 0U) {
                option.known_kind = XPS_HEADER_OPTION_NONE;
                option.payload_bytes = option.count * 4U;
            } else if (option.type_raw == 1U) {
                xps_u32 pad;

                option.known_kind = XPS_HEADER_OPTION_DEFAULT_POSE;
                pad = option.count & 3U;
                option.payload_bytes = option.count;
                if (pad != 0U) {
                    option.payload_bytes += (4U - pad);
                }
            } else if (option.type_raw == 2U) {
                option.known_kind = XPS_HEADER_OPTION_FLAGS;
                option.payload_bytes = option.count * 8U;
            } else {
                option.known_kind = XPS_HEADER_OPTION_UNKNOWN;
                local_header.unknown_option_count += 1U;
                remaining_bytes = settings_len_bytes - bytes_read;
                option.payload_bytes = remaining_bytes;
            }

            rc = xps_internal_invoke_header_option(sink, user, &option);
            if (rc != XPS_OK) {
                return rc;
            }

            if (option.payload_bytes > settings_len_bytes - bytes_read) {
                return XPS_ERR_PARSE;
            }

            if (option.known_kind == XPS_HEADER_OPTION_NONE) {
                rc = xps_internal_skip(fp, option.payload_bytes);
                if (rc != XPS_OK) {
                    return rc;
                }
            } else if (option.known_kind == XPS_HEADER_OPTION_DEFAULT_POSE) {
                rc = xps_header_parse_default_pose(fp,
                                                   option.count,
                                                   option.info,
                                                   sink,
                                                   user,
                                                   &local_header);
                if (rc != XPS_OK) {
                    return rc;
                }
            } else if (option.known_kind == XPS_HEADER_OPTION_FLAGS) {
                rc = xps_header_parse_flags(fp,
                                            option.count,
                                            sink,
                                            user,
                                            &flags,
                                            &local_header);
                if (rc != XPS_OK) {
                    return rc;
                }
            } else {
                rc = xps_internal_skip(fp, option.payload_bytes);
                if (rc != XPS_OK) {
                    return rc;
                }
                bytes_read += option.payload_bytes;
                break;
            }
            bytes_read += option.payload_bytes;
        }

        if (bytes_read < settings_len_bytes) {
            rc = xps_internal_skip(fp, settings_len_bytes - bytes_read);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        rc = xps_internal_invoke_header_flags(sink, user, &flags);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    *header = local_header;
    return xps_internal_invoke_header_end(sink, user, header);
}

int xps_header_detect_file(const char *path, xps_header_info *out_header)
{
    FILE *fp;
    int rc;

    if (path == NULL || out_header == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }
    rc = xps_internal_parse_xps_header(fp, NULL, NULL, out_header);
    fclose(fp);
    return rc;
}

int xps_header_parse_file(const char *path, const xps_header_sink *sink, void *user)
{
    FILE *fp;
    xps_header_info header;
    int rc;

    if (path == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }
    rc = xps_internal_parse_xps_header(fp, sink, user, &header);
    fclose(fp);
    return rc;
}
