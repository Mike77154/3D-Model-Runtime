#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>

#define RE4_PROBE_FILE_CAP (32UL * 1024UL * 1024UL)
#define RE4_PROBE_ARENA_CAP (4UL * 1024UL * 1024UL)

static re4_u8 g_file[RE4_PROBE_FILE_CAP];
static re4_u8 g_arena_mem[RE4_PROBE_ARENA_CAP];

static int has_ext(const char *path, const char *ext)
{
    re4_size lp;
    re4_size le;
    lp = (re4_size)strlen(path);
    le = (re4_size)strlen(ext);
    if (lp < le) return 0;
    return strcmp(path + lp - le, ext) == 0;
}

static int name_has(const char *path, const char *part)
{
    return strstr(path, part) != 0;
}

static re4_strref probe_next_line(re4_span text, re4_size *pos)
{
    re4_size start;
    re4_strref line;
    start = *pos;
    while (*pos < text.size && text.data[*pos] != (re4_u8)'\n') ++(*pos);
    line.ptr = (const char *)text.data + start;
    line.len = *pos - start;
    if (*pos < text.size && text.data[*pos] == (re4_u8)'\n') ++(*pos);
    if (line.len != 0UL && line.ptr[line.len - 1UL] == '\r') --line.len;
    return line;
}

static re4_strref probe_trim(re4_strref s)
{
    while (s.len != 0UL && (s.ptr[0] == ' ' || s.ptr[0] == '\t' || s.ptr[0] == '\r' || s.ptr[0] == '\n')) { ++s.ptr; --s.len; }
    while (s.len != 0UL) { char c; c = s.ptr[s.len - 1UL]; if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n')) break; --s.len; }
    return s;
}

static void print_str(re4_strref s)
{
    re4_size i;
    for (i = 0; i < s.len; ++i) putchar((int)s.ptr[i]);
}

static void print_named_ref(const char *label, re4_strref s)
{
    if (s.ptr != 0 && s.len != 0) {
        printf(" %s=", label);
        print_str(s);
    }
}

static void print_archive_summary(const char *label, const re4_archive *arc)
{
    re4_u32 i;
    printf("%s entries=%u flags=%u endian=%s\n", label,
           (unsigned)arc->entry_count,
           (unsigned)arc->flags,
           arc->endian == RE4_ENDIAN_BIG ? "big" : "little");
    for (i = 0; i < arc->entry_count && i < 12U; ++i) {
        printf("  %u: off=0x%X packed=%u unpacked=%u flags=0x%X",
               (unsigned)i,
               (unsigned)arc->entries[i].offset,
               (unsigned)arc->entries[i].packed_size,
               (unsigned)arc->entries[i].unpacked_size,
               (unsigned)arc->entries[i].flags);
        print_named_ref("name", arc->entries[i].name);
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    FILE *f;
    re4_size n;
    re4_span src;
    re4_arena arena;
    re4_err er;

    if (argc < 2) {
        printf("usage: re4_probe file\n");
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (f == 0) {
        printf("open failed\n");
        return 2;
    }
    n = fread(g_file, 1, sizeof(g_file), f);
    fclose(f);
    src.data = g_file;
    src.size = n;
    re4_arena_init(&arena, g_arena_mem, sizeof(g_arena_mem));


    if (name_has(argv[1], ".idxps2tpl")) {
        re4_idxps2tpl doc;
        er = re4_idxps2tpl_read_text(src, &doc);
        printf("IDXPS2TPL parse: %s flip_y=%u seen=0x%X",
               re4_err_name(er),
               (unsigned)doc.image_flip_y,
               (unsigned)doc.seen_mask);
        if (er == RE4_OK) {
            print_named_ref("folder", doc.image_folder);
            print_named_ref("format", doc.image_format);
        }
        printf("\n");
        return er == RE4_OK ? 0 : 3;
    }

    if (name_has(argv[1], ".IdxtplHeader") || name_has(argv[1], ".idxtplheader")) {
        re4_idxtpl_header h;
        re4_u32 warnings;
        er = re4_idxtpl_header_read_text(src, &h);
        warnings = 0U;
        if (er == RE4_OK) (void)re4_idxtpl_header_validate_basic(&h, &warnings);
        printf("IdxtplHeader parse: %s bitdepth=%u colors=%u interlace=0x%X rotate=%u swizzle=%u size=%ux%u next=0x%X mip=0x%X qwc=0x%X ref=0x%X idx=0x%X pal=0x%X gstex=%08X:%08X warnings=0x%X seen=0x%X\n",
               re4_err_name(er),
               (unsigned)h.bitdepth,
               (unsigned)re4_ps2tpl_bitdepth_palette_limit(h.bitdepth),
               (unsigned)h.interlace,
               (unsigned)re4_idxtpl_header_has_interlace(&h, RE4_PS2TPL_INTERLACE_ROTATE_IMAGE),
               (unsigned)re4_idxtpl_header_has_interlace(&h, RE4_PS2TPL_INTERLACE_SWIZZLE),
               (unsigned)h.width,
               (unsigned)h.height,
               (unsigned)h.next,
               (unsigned)h.mipmap_status,
               (unsigned)h.qwc,
               (unsigned)h.ref,
               (unsigned)h.indexes_offset,
               (unsigned)h.palette_offset,
               (unsigned)h.gs_tex_hi,
               (unsigned)h.gs_tex,
               (unsigned)warnings,
               (unsigned)h.seen_mask);
        return er == RE4_OK ? 0 : 3;
    }

    if (name_has(argv[1], ".idxps2bin")) {
        re4_idxps2bin doc;
        re4_u32 i;
        er = re4_idxps2bin_read_text(src, &arena, &doc);
        printf("IDXPS2BIN parse: %s kv=%u seen=0x%X use_material=%u auto_bbox=%u mats=%u bones=%u weights=%u\n",
               re4_err_name(er),
               (unsigned)doc.kv_count,
               (unsigned)doc.seen_mask,
               (unsigned)doc.use_idxmaterial,
               (unsigned)doc.auto_calc_bounding_box,
               (unsigned)doc.material_count,
               (unsigned)doc.bone_count,
               (unsigned)doc.weightmap_count);
        if (er == RE4_OK) {
            for (i = 0U; i < doc.kv_count && i < 24U; ++i) {
                printf("  key="); print_str(doc.kvs[i].key);
                printf(" value="); print_str(doc.kvs[i].value);
                printf(" flags=0x%X u32=%u q16=%ld\n",
                       (unsigned)doc.kvs[i].flags,
                       (unsigned)doc.kvs[i].value_u32,
                       (long)doc.kvs[i].value_q16);
            }
        }
        return er == RE4_OK ? 0 : 3;
    }

    if (name_has(argv[1], ".idxr100extract") || name_has(argv[1], ".idxuur100repack") || name_has(argv[1], ".idxggr100repack")) {
        re4_scenario_r100 r100;
        re4_u32 i;
        er = re4_scenario_r100_read_text(src, &arena, &r100);
        printf("scenario r100 parse: %s params=%u extra_smd=%u extra_tpl=%u\n",
               re4_err_name(er),
               (unsigned)r100.extra_parameter_count,
               (unsigned)r100.extra_smd_count,
               (unsigned)r100.extra_tpl_count);
        if (er == RE4_OK) {
            for (i = 0U; i < r100.extra_parameter_count && i < 16U; ++i) {
                printf("  ExtraParameter_%u=%u\n", (unsigned)r100.extra_parameters[i].id, (unsigned)r100.extra_parameters[i].value);
            }
            for (i = 0U; i < r100.extra_smd_count && i < 16U; ++i) {
                printf("  ExtraSmdFileName_%u=", (unsigned)r100.extra_smd_files[i].id); print_str(r100.extra_smd_files[i].name); printf("\n");
            }
            for (i = 0U; i < r100.extra_tpl_count && i < 16U; ++i) {
                printf("  ExtraTplFileName_%u=", (unsigned)r100.extra_tpl_files[i].id); print_str(r100.extra_tpl_files[i].name); printf("\n");
            }
        }
        return er == RE4_OK ? 0 : 3;
    }

    if (name_has(argv[1], ".idxuhdtpl")) {
        re4_idxuhdtpl idx;
        re4_u32 i;
        er = re4_idxuhdtpl_read_text(src, &arena, &idx);
        printf("IDXUHDTPL parse: %s entries=%u\n", re4_err_name(er), (unsigned)idx.entry_count);
        if (er == RE4_OK) {
            for (i = 0U; i < idx.entry_count && i < 16U; ++i) {
                printf("  TPL_%03u pack=0x%08X texture=%u fmt=0x%02X size=%ux%u seen=0x%X\n",
                       (unsigned)idx.entries[i].id,
                       (unsigned)idx.entries[i].pack_id,
                       (unsigned)idx.entries[i].texture_id,
                       (unsigned)idx.entries[i].pixel_format_type,
                       (unsigned)idx.entries[i].width,
                       (unsigned)idx.entries[i].height,
                       (unsigned)idx.entries[i].seen_mask);
            }
        }
        return er == RE4_OK ? 0 : 3;
    }

    if (name_has(argv[1], ".idxmaterial")) {
        re4_idxmaterial idx;
        re4_u32 i;
        er = re4_idxmaterial_read_text(src, &arena, &idx);
        printf("IDXMATERIAL parse: %s entries=%u\n", re4_err_name(er), (unsigned)idx.entry_count);
        if (er == RE4_OK) {
            for (i = 0U; i < idx.entry_count && i < 16U; ++i) {
                printf("  %u: name=", (unsigned)i);
                print_str(idx.entries[i].name);
                printf(" flag=0x%02X diffuse=%u bump=%u opacity=%u generic=%u custom=%u spec=%u,%u,%u scale=0x%02X\n",
                       (unsigned)idx.entries[i].core.material_flag,
                       (unsigned)idx.entries[i].core.diffuse_map,
                       (unsigned)idx.entries[i].core.bump_map,
                       (unsigned)idx.entries[i].core.opacity_map,
                       (unsigned)idx.entries[i].core.generic_specular_map,
                       (unsigned)idx.entries[i].core.custom_specular_map,
                       (unsigned)idx.entries[i].core.specular_rgb[0],
                       (unsigned)idx.entries[i].core.specular_rgb[1],
                       (unsigned)idx.entries[i].core.specular_rgb[2],
                       (unsigned)idx.entries[i].core.specular_scale);
            }
        }
        return er == RE4_OK ? 0 : 3;
    }


    if (name_has(argv[1], ".idxuubin")) {
        re4_idxuubin doc;
        re4_u32 i;
        er = re4_idxuubin_read_text(src, &arena, &doc);
        printf("IDXUUBIN parse: %s kv=%u seen=0x%X vcol=%u mats=%u bones=%u weights=%u tex=%u morph=%u\n",
               re4_err_name(er),
               (unsigned)doc.kv_count,
               (unsigned)doc.seen_mask,
               (unsigned)doc.use_vertex_color,
               (unsigned)doc.material_count,
               (unsigned)doc.bone_count,
               (unsigned)doc.weightmap_count,
               (unsigned)doc.texture_count,
               (unsigned)doc.morph_count);
        if (er == RE4_OK) {
            for (i = 0U; i < doc.kv_count && i < 24U; ++i) {
                printf("  line=%u ", (unsigned)doc.kvs[i].line_no);
                if (doc.kvs[i].section.ptr != 0 && doc.kvs[i].section.len != 0UL) {
                    printf("section="); print_str(doc.kvs[i].section); printf(" ");
                }
                printf("key="); print_str(doc.kvs[i].key);
                printf(" value="); print_str(doc.kvs[i].value);
                printf(" flags=0x%X u32=%u q16=%ld\n",
                       (unsigned)doc.kvs[i].flags,
                       (unsigned)doc.kvs[i].value_u32,
                       (long)doc.kvs[i].value_q16);
            }
        }
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".mtl") || has_ext(argv[1], ".MTL")) {
        re4_mtl mtl;
        re4_idxuhdtpl tplidx;
        re4_idxmaterial midx;
        re4_u32 i;
        er = re4_mtl_read_text(src, &arena, &mtl);
        printf("MTL parse: %s materials=%u\n", re4_err_name(er), (unsigned)mtl.material_count);
        if (er == RE4_OK) {
            for (i = 0U; i < mtl.material_count && i < 16U; ++i) {
                printf("  material %u name=", (unsigned)i);
                print_str(mtl.materials[i].name);
                printf(" seen=0x%X Ks=%u,%u,%u\n",
                       (unsigned)mtl.materials[i].seen_mask,
                       (unsigned)mtl.materials[i].specular_rgb[0],
                       (unsigned)mtl.materials[i].specular_rgb[1],
                       (unsigned)mtl.materials[i].specular_rgb[2]);
                if ((mtl.materials[i].seen_mask & RE4_MTL_MATERIAL_SEEN_DIFFUSE) != 0U) {
                    printf("    diffuse pack=0x%08X texture=%u pft=0x%02X\n",
                           (unsigned)mtl.materials[i].diffuse.pack_id,
                           (unsigned)mtl.materials[i].diffuse.texture_id,
                           (unsigned)re4_mtl_pixel_format_type_for_ref(&mtl.materials[i].diffuse));
                }
                if ((mtl.materials[i].seen_mask & RE4_MTL_MATERIAL_SEEN_BUMP) != 0U) {
                    printf("    bump pack=0x%08X texture=%u pft=0x%02X\n",
                           (unsigned)mtl.materials[i].bump.pack_id,
                           (unsigned)mtl.materials[i].bump.texture_id,
                           (unsigned)re4_mtl_pixel_format_type_for_ref(&mtl.materials[i].bump));
                }
                if ((mtl.materials[i].seen_mask & RE4_MTL_MATERIAL_SEEN_OPACITY) != 0U) {
                    printf("    opacity pack=0x%08X texture=%u pft=0x%02X\n",
                           (unsigned)mtl.materials[i].opacity.pack_id,
                           (unsigned)mtl.materials[i].opacity.texture_id,
                           (unsigned)re4_mtl_pixel_format_type_for_ref(&mtl.materials[i].opacity));
                }
                if ((mtl.materials[i].seen_mask & RE4_MTL_MATERIAL_SEEN_SPECULAR) != 0U) {
                    printf("    spec pack=0x%08X texture=%u pft=0x%02X scale=%u/%u\n",
                           (unsigned)mtl.materials[i].specular.pack_id,
                           (unsigned)mtl.materials[i].specular.texture_id,
                           (unsigned)re4_mtl_pixel_format_type_for_ref(&mtl.materials[i].specular),
                           (unsigned)mtl.materials[i].specular.scale_x_units,
                           (unsigned)mtl.materials[i].specular.scale_y_units);
                }
            }
            er = re4_mtl_collect_idxuhdtpl(&mtl, 1U, 1U, &arena, &tplidx);
            printf("MTL -> IDXUHDTPL: %s entries=%u\n", re4_err_name(er), (unsigned)tplidx.entry_count);
            if (er == RE4_OK) {
                er = re4_idxmaterial_from_mtl(&mtl, &tplidx, &arena, &midx);
                printf("MTL -> IDXMATERIAL: %s entries=%u\n", re4_err_name(er), (unsigned)midx.entry_count);
                if (er == RE4_OK) {
                    for (i = 0U; i < midx.entry_count && i < 16U; ++i) {
                        printf("  matcore %u flag=0x%02X diffuse=%u bump=%u opacity=%u generic=%u custom=%u scale=0x%02X\n",
                               (unsigned)i,
                               (unsigned)midx.entries[i].core.material_flag,
                               (unsigned)midx.entries[i].core.diffuse_map,
                               (unsigned)midx.entries[i].core.bump_map,
                               (unsigned)midx.entries[i].core.opacity_map,
                               (unsigned)midx.entries[i].core.generic_specular_map,
                               (unsigned)midx.entries[i].core.custom_specular_map,
                               (unsigned)midx.entries[i].core.specular_scale);
                    }
                }
            }
        }
        return 0;
    }

    if (name_has(argv[1], ".idxuuscenario") || name_has(argv[1], ".idxggscenario") || name_has(argv[1], ".idxuusmd") || name_has(argv[1], ".idxggsmd")) {
        re4_scenario_idx_doc doc;
        re4_u32 i;
        er = re4_scenario_idx_read_text(src, &arena, &doc);
        printf("scenario idx parse: %s magic=0x%X use_tpl=%u use_material=%u vcol=%u dyn_vcol=%u entries=%u extra=%u files=%u tplfiles=%u\n",
               re4_err_name(er),
               (unsigned)doc.options.magic,
               (unsigned)doc.options.use_idxuhdtpl,
               (unsigned)doc.options.use_idxmaterial,
               (unsigned)doc.options.enable_vertex_color,
               (unsigned)doc.options.enable_dynamic_vertex_color,
               (unsigned)doc.entry_count,
               (unsigned)doc.extra_parameter_count,
               (unsigned)doc.extra_file_name_count,
               (unsigned)doc.extra_tpl_file_name_count);
        if (er == RE4_OK) {
            print_named_ref("smd", doc.options.smd_file_name);
            print_named_ref("tpl", doc.options.tpl_file_name);
            print_named_ref("bin_folder", doc.options.bin_folder);
            print_named_ref("shared", doc.options.shared_file_name);
            printf("\n");
            for (i = 0U; i < doc.entry_count && i < 16U; ++i) {
                printf("  entry %u file=%u smd=%u pos=(%ld,%ld,%ld) scale=(%ld,%ld,%ld) tpl=%u bin=%u smx=%u status=0x%02X seen=0x%X\n",
                       (unsigned)i,
                       (unsigned)doc.entries[i].file_id,
                       (unsigned)doc.entries[i].smd_id,
                       (long)doc.entries[i].position[0],
                       (long)doc.entries[i].position[1],
                       (long)doc.entries[i].position[2],
                       (long)doc.entries[i].scale[0],
                       (long)doc.entries[i].scale[1],
                       (long)doc.entries[i].scale[2],
                       (unsigned)doc.entries[i].tpl_file_id,
                       (unsigned)doc.entries[i].bin_file_id,
                       (unsigned)doc.entries[i].smx_id,
                       (unsigned)doc.entries[i].object_status,
                       (unsigned)doc.entries[i].seen_mask);
            }
        }
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".obj") || has_ext(argv[1], ".OBJ")) {
        re4_size pos;
        re4_u32 hits;
        pos = 0UL;
        hits = 0U;
        while (pos < src.size) {
            re4_strref line;
            re4_scenario_group_ref group;
            line = probe_next_line(src, &pos);
            line = probe_trim(line);
            if (line.len > 2UL && line.ptr[0] == 'g' && (line.ptr[1] == ' ' || line.ptr[1] == '\t')) {
                line.ptr += 2;
                line.len -= 2;
                line = probe_trim(line);
                if (re4_scenario_group_name_parse(line, &group) == RE4_OK) {
                    printf("OBJ scenario group %u: prefix=%s align=%u file=%u smd=%u smx=%u type=0x%02X bin=%u seen=0x%X\n",
                           (unsigned)hits,
                           re4_scenario_prefix_name(group.prefix),
                           (unsigned)re4_scenario_alignment_for_prefix(group.prefix),
                           (unsigned)group.file_id,
                           (unsigned)group.smd_id,
                           (unsigned)group.smx_id,
                           (unsigned)group.object_status,
                           (unsigned)group.bin_id,
                           (unsigned)group.seen_mask);
                    ++hits;
                    if (hits >= 24U) break;
                }
            }
        }
        printf("OBJ scenario groups=%u\n", (unsigned)hits);
        return 0;
    }

    if (has_ext(argv[1], ".TPL") || has_ext(argv[1], ".tpl")) {
        re4_tpl tpl;
        er = re4_tpl_read_gc(src, &arena, &tpl);
        if (er == RE4_OK) {
            re4_u32 i;
            printf("TPL GC images=%u\n", (unsigned)tpl.image_count);
            for (i = 0; i < tpl.image_count; ++i) {
                re4_size payload_bytes;
                re4_size pal_bytes;
                re4_u32 bw;
                re4_u32 bh;
                re4_u32 bb;
                if (re4_tpl_image_payload_size(&tpl.images[i], &payload_bytes) != RE4_OK) payload_bytes = 0;
                if (re4_tpl_palette_payload_size(&tpl.images[i], &pal_bytes) != RE4_OK) pal_bytes = 0;
                if (re4_tpl_format_block_info(tpl.images[i].format, &bw, &bh, &bb) != RE4_OK) { bw = 0; bh = 0; bb = 0; }
                printf("  %u: %ux%u %s block=%ux%u/%u data=0x%X bytes=%lu pal=%lu indexed=%u\n", (unsigned)i,
                       (unsigned)tpl.images[i].width,
                       (unsigned)tpl.images[i].height,
                       re4_tpl_format_name(tpl.images[i].format),
                       (unsigned)bw, (unsigned)bh, (unsigned)bb,
                       (unsigned)tpl.images[i].data_offset,
                       (unsigned long)payload_bytes,
                       (unsigned long)pal_bytes,
                       (unsigned)re4_tpl_format_is_indexed(tpl.images[i].format));
            }
            return 0;
        }
        re4_arena_reset(&arena);
        {
            re4_uhd_tpl utpl;
            er = re4_uhd_tpl_read(src, RE4_ENDIAN_LITTLE, RE4_FALSE, &arena, &utpl);
            if (er == RE4_OK) {
                re4_u32 i;
                printf("TPL UHD images=%u table=0x%X\n", (unsigned)utpl.image_count, (unsigned)utpl.table_offset);
                for (i = 0; i < utpl.image_count && i < 12U; ++i) {
                    printf("  %u: %ux%u fmt=%u pack=0x%X tex=%u pal=%u comp=%u\n",
                           (unsigned)i,
                           (unsigned)utpl.images[i].width,
                           (unsigned)utpl.images[i].height,
                           (unsigned)utpl.images[i].pixel_format,
                           (unsigned)utpl.images[i].pack_id,
                           (unsigned)utpl.images[i].texture_id,
                           (unsigned)utpl.images[i].has_palette,
                           (unsigned)utpl.images[i].is_compressed);
                }
                return 0;
            }
        }
        re4_arena_reset(&arena);
        {
            re4_ps2_tpl ptpl;
            er = re4_ps2_tpl_read(src, RE4_ENDIAN_LITTLE, &arena, &ptpl);
            if (er == RE4_OK) {
                re4_u32 i;
                printf("TPL PS2 images=%u start=0x%X magic=0x%X\n",
                       (unsigned)ptpl.image_count,
                       (unsigned)ptpl.start_offset,
                       (unsigned)ptpl.magic);
                for (i = 0U; i < ptpl.image_count && i < 12U; ++i) {
                    re4_span idxspan;
                    re4_span palspan;
                    re4_u32 warnings;
                    re4_idxtpl_header ih;
                    (void)re4_ps2_tpl_indexes_span(src, &ptpl.images[i].main_header, &idxspan);
                    (void)re4_ps2_tpl_palette_span(src, &ptpl.images[i].main_header, &palspan);
                    (void)re4_ps2_tpl_main_header_as_idxtpl(&ptpl.images[i], &ih);
                    warnings = 0U;
                    (void)re4_idxtpl_header_validate_basic(&ih, &warnings);
                    printf("  %u: %ux%u bitdepth=%u colors=%u interlace=0x%X qwc=%u idx=0x%X/%lu pal=0x%X/%lu mip=%u warnings=0x%X\n",
                           (unsigned)i,
                           (unsigned)ptpl.images[i].main_header.width,
                           (unsigned)ptpl.images[i].main_header.height,
                           (unsigned)ptpl.images[i].main_header.bitdepth,
                           (unsigned)re4_ps2tpl_bitdepth_palette_limit((re4_u32)ptpl.images[i].main_header.bitdepth),
                           (unsigned)ptpl.images[i].main_header.interlace,
                           (unsigned)ptpl.images[i].main_header.qwc,
                           (unsigned)ptpl.images[i].main_header.indexes_offset,
                           (unsigned long)idxspan.size,
                           (unsigned)ptpl.images[i].main_header.palette_offset,
                           (unsigned long)palspan.size,
                           (unsigned)re4_ps2_tpl_image_has_mipmaps(&ptpl.images[i]),
                           (unsigned)warnings);
                }
                return 0;
            }
        }
        re4_arena_reset(&arena);
        {
            re4_tplcue cue;
            er = re4_tplcue_read_le(src, &arena, &cue);
            printf("TPL cue parse: %s records=%u\n", re4_err_name(er), (unsigned)cue.record_count);
            if (er == RE4_OK) {
                re4_u32 i;
                for (i = 0U; i < cue.record_count && i < 12U; ++i) {
                    printf("  %u: pack=0x%08X texture=%u raw=(0x%08X,0x%08X)\n",
                           (unsigned)i,
                           (unsigned)re4_tplcue_record_pack_id(&cue.records[i]),
                           (unsigned)re4_tplcue_record_texture_id(&cue.records[i]),
                           (unsigned)cue.records[i].a,
                           (unsigned)cue.records[i].b);
                }
            }
            return er == RE4_OK ? 0 : 3;
        }
    }

    if (has_ext(argv[1], ".EVD") || has_ext(argv[1], ".evd")) {
        re4_evd evd;
        er = re4_evd_read(src, RE4_ENDIAN_LITTLE, &arena, &evd);
        printf("EVD parse: %s\n", re4_err_name(er));
        if (er == RE4_OK) {
            re4_u32 i;
            re4_evd_manifest manifest;
            printf("name="); print_str(evd.name_file); printf(" packets=%u files=%u\n", (unsigned)evd.packet_count, (unsigned)evd.num_bin_tbl);
            if (re4_evd_build_manifest(&evd, &manifest) == RE4_OK) {
                printf(" manifest files BIN=%u TPL=%u FCV=%u SEQ=%u LIT=%u EFF=%u SMD=%u MDT=%u refs model=%u bin=%u tpl=%u\n",
                       (unsigned)manifest.file_counts[RE4_EVD_FILE_BIN],
                       (unsigned)manifest.file_counts[RE4_EVD_FILE_TPL],
                       (unsigned)manifest.file_counts[RE4_EVD_FILE_FCV],
                       (unsigned)manifest.file_counts[RE4_EVD_FILE_SEQ],
                       (unsigned)manifest.file_counts[RE4_EVD_FILE_LIT],
                       (unsigned)manifest.file_counts[RE4_EVD_FILE_EFF],
                       (unsigned)manifest.file_counts[RE4_EVD_FILE_SMD],
                       (unsigned)manifest.file_counts[RE4_EVD_FILE_MDT],
                       (unsigned)manifest.model_ref_count,
                       (unsigned)manifest.bin_ref_count,
                       (unsigned)manifest.tpl_ref_count);
            }
            for (i = 0; i < evd.packet_count && i < 16U; ++i) {
                re4_evd_resource_ref rr;
                printf("  packet %u: %s size=%d frame=%d", (unsigned)i, re4_evp_type_name(evd.packets[i].type), (int)evd.packets[i].size, (int)evd.packets[i].frame);
                if (re4_evd_packet_resource_ref_e(&evd.packets[i], evd.endian, &rr) == RE4_OK) {
                    print_named_ref("model", rr.model_name);
                    print_named_ref("parent", rr.parent_name);
                    print_named_ref("bin", rr.bin_name);
                    print_named_ref("tpl", rr.tpl_name);
                }
                printf("\n");
            }
        }
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".LFS") || has_ext(argv[1], ".lfs")) {
        re4_lfs lfs;
        er = re4_lfs_read(src, &arena, &lfs);
        printf("LFS parse: %s chunks=%u unpacked=%u endian=%s\n", re4_err_name(er), (unsigned)lfs.chunk_count, (unsigned)lfs.size_decompressed, lfs.endian == RE4_ENDIAN_BIG ? "big" : "little");
        if (er == RE4_OK && lfs.chunk_count != 0U) {
            re4_span chunk;
            re4_size unpacked;
            if (re4_lfs_chunk_span(src, &lfs.chunks[0], &chunk, &unpacked) == RE4_OK) {
                printf("  first chunk off=0x%lX packed=%lu unpacked=%lu compressed=%u\n",
                       (unsigned long)(chunk.data - src.data),
                       (unsigned long)chunk.size,
                       (unsigned long)unpacked,
                       (unsigned)lfs.chunks[0].compressed);
            }
        }
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".AFS") || has_ext(argv[1], ".afs")) {
        re4_archive afs;
        er = re4_afs_read(src, &arena, &afs);
        printf("AFS parse: %s\n", re4_err_name(er));
        if (er == RE4_OK) print_archive_summary("AFS", &afs);
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".GCA") || has_ext(argv[1], ".gca")) {
        re4_archive gca;
        er = re4_gca_read(src, &arena, &gca);
        printf("GCA parse: %s\n", re4_err_name(er));
        if (er == RE4_OK) print_archive_summary("GCA", &gca);
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".DAT") || has_ext(argv[1], ".dat")) {
        re4_archive dat;
        if (src.size >= 4UL && memcmp(src.data, "GCAX", 4UL) == 0) {
            er = re4_gca_read(src, &arena, &dat);
            printf("GCAX archive: %s\n", re4_err_name(er));
        } else {
            er = re4_dat_read_indexed(src, RE4_ENDIAN_LITTLE, RE4_FALSE,
                                      &arena, &dat);
            printf("DAT indexed: %s\n", re4_err_name(er));
        }
        if (er == RE4_OK) print_archive_summary("DAT", &dat);
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".UDAS") || has_ext(argv[1], ".udas")) {
        re4_archive udas;
        er = re4_udas_read_guess(src, RE4_ENDIAN_LITTLE, &arena, &udas);
        printf("UDAS guess: %s\n", re4_err_name(er));
        if (er == RE4_OK) print_archive_summary("UDAS", &udas);
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".ESL") || has_ext(argv[1], ".esl")) {
        re4_esl esl;
        er = re4_esl_read(src, &arena, &esl);
        printf("ESL parse: %s entries=%u\n", re4_err_name(er), (unsigned)esl.entry_count);
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".FCV") || has_ext(argv[1], ".fcv")) {
        re4_fcv fcv;
        er = re4_fcv_read(src, RE4_ENDIAN_LITTLE, &arena, &fcv);
        printf("FCV parse: %s max_time=%u blocks=%u\n", re4_err_name(er), (unsigned)fcv.max_time, (unsigned)fcv.block_count);
        if (er == RE4_OK && fcv.block_count > 0U) {
            re4_fcv_axis_view ax;
            re4_fcv_axis_view ay;
            re4_fcv_axis_view az;
            re4_err aer;
            aer = re4_fcv_block_axis_views(src, &fcv.blocks[0], &ax, &ay, &az);
            printf(" block0 bone=%u %s data=0x%02X tuple=%u axis=[%u,%u,%u] views=%s\n",
                   (unsigned)fcv.blocks[0].bone_no,
                   re4_fcv_bone_type_name(fcv.blocks[0].bone_type),
                   (unsigned)fcv.blocks[0].data_type,
                   (unsigned)fcv.blocks[0].value_bytes_per_key,
                   (unsigned)fcv.blocks[0].axis_value_size[0],
                   (unsigned)fcv.blocks[0].axis_value_size[1],
                   (unsigned)fcv.blocks[0].axis_value_size[2],
                   re4_err_name(aer));
            if (aer == RE4_OK) {
                re4_q16 sample[3];
                printf("  X count=%u scalar=%s Y count=%u scalar=%s Z count=%u scalar=%s\n",
                       (unsigned)ax.tag_count, re4_fcv_scalar_name(ax.value_size),
                       (unsigned)ay.tag_count, re4_fcv_scalar_name(ay.value_size),
                       (unsigned)az.tag_count, re4_fcv_scalar_name(az.value_size));
                if (re4_fcv_block_sample_nearest_q16(src, &fcv.blocks[0], 0U, RE4_ENDIAN_LITTLE, sample) == RE4_OK) {
                    printf("  sample@0 q16=(%ld,%ld,%ld)\n", (long)sample[0], (long)sample[1], (long)sample[2]);
                }
            }
        }
        return er == RE4_OK ? 0 : 3;
    }


    if (has_ext(argv[1], ".SMD") || has_ext(argv[1], ".smd")) {
        re4_smd_room room;
        er = re4_smd_room_read(src, RE4_ENDIAN_LITTLE, &arena, &room);
        printf("SMD room parse: %s lines=%u bin_off=0x%X tpl_off=0x%X tail=0x%X\n",
               re4_err_name(er),
               (unsigned)room.line_count,
               (unsigned)room.offset_bin_array,
               (unsigned)room.offset_tpl_array,
               (unsigned)room.first_bin_or_file_end);
        if (er == RE4_OK && room.line_count != 0U) {
            printf("  line0 bin=%u tpl=%u smx=%u status=0x%X tpl_ref=%u smx_ref=%u\n",
                   (unsigned)room.lines[0].bin_file_id,
                   (unsigned)room.lines[0].tpl_file_id,
                   (unsigned)room.lines[0].smx_id,
                   (unsigned)room.lines[0].object_status,
                   (unsigned)re4_smd_line_references_tpl(&room.lines[0]),
                   (unsigned)re4_smd_line_references_smx(&room.lines[0]));
        }
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".PMD") || has_ext(argv[1], ".pmd")) {
        re4_pmd_model pmd;
        er = re4_pmd_read_survey(src, &arena, &pmd);
        printf("PMD survey: %s skel_names=%u node_names=%u skeletons=%u nodes=%u meshes=%u materials=%u vertices~%u indices~%u\n",
               re4_err_name(er),
               (unsigned)pmd.skeleton_name_count,
               (unsigned)pmd.node_group_name_count,
               (unsigned)pmd.skeleton_count,
               (unsigned)pmd.node_count,
               (unsigned)pmd.total_mesh_count,
               (unsigned)pmd.material_count,
               (unsigned)pmd.total_vertex_count_hint,
               (unsigned)pmd.total_index_count_hint);
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".SMX") || has_ext(argv[1], ".smx")) {
        re4_smx_table smx;
        er = re4_smx_table_read(src, RE4_ENDIAN_LITTLE, &arena, &smx);
        printf("SMX parse: %s records=%u\n", re4_err_name(er), (unsigned)smx.record_count);
        if (er == RE4_OK && smx.record_count != 0U) {
            printf("  rec0 id=%u mode=%u light=0x%X color=(%u,%u,%u,%u) texmove=(%ld,%ld)\n",
                   (unsigned)smx.records[0].use_smx_id,
                   (unsigned)smx.records[0].mode,
                   (unsigned)smx.records[0].light_switch,
                   (unsigned)smx.records[0].color_rgb[0],
                   (unsigned)smx.records[0].color_rgb[1],
                   (unsigned)smx.records[0].color_rgb[2],
                   (unsigned)smx.records[0].color_alpha,
                   (long)smx.records[0].texture_movement[0],
                   (long)smx.records[0].texture_movement[1]);
        }
        return er == RE4_OK ? 0 : 3;
    }

    if (has_ext(argv[1], ".BIN") || has_ext(argv[1], ".bin")) {
        re4_bin_model bm;
        /* Distinct GC (big-endian) and PS2 (little-endian) BIN signatures. */
        if (src.size >= 0x40U &&
            (re4_read_u32_be(src.data + 0x3CU) == 0x20010801U ||
             re4_read_u32_be(src.data + 0x3CU) == 0x20030817U ||
             re4_read_u32_be(src.data + 0x3CU) == 0x20030818U)) {
            re4_gc_bin gc;
            re4_u32 i;
            er = re4_gc_bin_read(src, &gc);
            printf("GameCube BIN: %s", re4_err_name(er));
            if (er == RE4_OK) {
                printf(" version=0x%08X polygons=%u vertices=%u normals=%u colors~%u uvs~%u\n",
                       (unsigned)gc.version, (unsigned)gc.polygon_count,
                       (unsigned)gc.positions_count, (unsigned)gc.normals_count,
                       (unsigned)gc.colors_count, (unsigned)gc.texcoords_count);
                for (i = 0U; i < gc.polygon_count; ++i) {
                    re4_gc_polygon part;
                    re4_gc_gx_counts counts;
                    re4_err er2;
                    er2 = re4_gc_bin_polygon(&gc, i, &part);
                    if (er2 == RE4_OK)
                        er2 = re4_gc_gx_decode(part.fifo,
                            (gc.flags & 0x80000000U) ? RE4_TRUE : RE4_FALSE,
                            gc.positions_count, gc.normals_count,
                            gc.colors_count, gc.texcoords_count,
                            0, 0U, 0, 0U, &counts);
                    if (i < 20U) {
                        printf("  part %u: %s", (unsigned)i, re4_err_name(er2));
                        if (er2 == RE4_OK)
                            printf(" corners=%u triangles=%u texture=%u flag=0x%02X",
                                   (unsigned)counts.corners, (unsigned)counts.triangles,
                                   (unsigned)part.diffuse_texture, (unsigned)part.flags);
                        printf("\n");
                    }
                    if (er2 != RE4_OK) return 3;
                }
            } else printf("\n");
            return er == RE4_OK ? 0 : 3;
        }
        if (src.size >= 0x50U && re4_read_u16_le(src.data) == 0x0030U) {
            re4_ps2_bin ps2;
            re4_u32 i, j, faces, total;
            re4_err er2;
            er = re4_ps2_bin_read(src, &ps2);
            printf("PS2 BIN: %s", re4_err_name(er));
            if (er != RE4_OK) { printf("\n"); return 3; }
            printf(" materials=%u bones=%u textures=%u\n",
                   (unsigned)ps2.material_count, (unsigned)ps2.bone_count,
                   (unsigned)ps2.texture_count);
            total = 0U;
            for (i = 0U; i < ps2.material_count; ++i) {
                re4_ps2_node node;
                er2 = re4_ps2_bin_node(&ps2, i, &node);
                if (er2 != RE4_OK) { printf("  node %u: %s\n", (unsigned)i, re4_err_name(er2)); return 3; }
                for (j = 0U; j < node.segments; ++j) {
                    re4_ps2_segment seg;
                    er2 = re4_ps2_node_segment(&ps2, &node, j, &seg);
                    if (er2 == RE4_OK) er2 = re4_ps2_segment_triangles(&ps2, &seg, 0, 0U, &faces);
                    if (er2 != RE4_OK) { printf("  segment %u/%u: %s\n", (unsigned)i,(unsigned)j,re4_err_name(er2)); return 3; }
                    if (RE4_U32_MAX - total < faces) return 3;
                    total += faces;
                    if (i < 8U && j < 8U)
                        printf("  node %u segment %u: vertices=%u faces=%u colors=%u\n",
                               (unsigned)i, (unsigned)j, (unsigned)seg.vertex_count,
                               (unsigned)faces, (unsigned)seg.scenario_color);
                }
            }
            printf("  total triangles=%u\n", (unsigned)total);
            return 0;
        }
        er = re4_bin_model_read(src, RE4_ENDIAN_LITTLE, &arena, &bm);
        if (er == RE4_OK) {
            printf("BIN model: materials=%u strips=%u bones=%u weights=%u bonepairs=%u vertices~%u tris~%u flags=0x%X\n",
                   (unsigned)bm.material_count,
                   (unsigned)bm.strip_count,
                   (unsigned)bm.bone_count,
                   (unsigned)bm.weightmap_count,
                   (unsigned)bm.bonepair_count,
                   (unsigned)bm.true_vertex_count,
                   (unsigned)bm.triangle_count_hint,
                   (unsigned)bm.header.flags);
            if (bm.material_count != 0U) {
                re4_bin_material_core core;
                re4_span mat_span;
                mat_span.data = bm.materials[0].raw;
                mat_span.size = 24UL;
                if (re4_bin_material_core_read_uhd24(mat_span, &core) == RE4_OK) {
                    printf("  mat0 flag=0x%02X diffuse=%u bump=%u opacity=%u custom=%u strips=%u verts~%u tris~%u\n",
                           (unsigned)core.material_flag,
                           (unsigned)core.diffuse_map,
                           (unsigned)core.bump_map,
                           (unsigned)core.opacity_map,
                           (unsigned)core.custom_specular_map,
                           (unsigned)bm.materials[0].strip_count,
                           (unsigned)bm.materials[0].vertex_count_hint,
                           (unsigned)bm.materials[0].triangle_count_hint);
                } else {
                    printf("  mat0 diffuse=%u bump=%u opacity=%u strips=%u verts~%u tris~%u\n",
                           (unsigned)bm.materials[0].diffuse_map,
                           (unsigned)bm.materials[0].bump_map,
                           (unsigned)bm.materials[0].opacity_map,
                           (unsigned)bm.materials[0].strip_count,
                           (unsigned)bm.materials[0].vertex_count_hint,
                           (unsigned)bm.materials[0].triangle_count_hint);
                }
            }
            return 0;
        }
        {
            re4_model_probe mp;
            er = re4_bin_model_probe(src, &mp);
            printf("BIN probe: %s size=%u offsets~%u strings~%u tpl~%u fcv~%u\n", re4_err_name(er), (unsigned)mp.file_size, (unsigned)mp.probable_offset_count, (unsigned)mp.probable_string_count, (unsigned)mp.probable_tpl_count, (unsigned)mp.probable_fcv_count);
            return er == RE4_OK ? 0 : 3;
        }
    }

    printf("Unknown extension; loaded %lu bytes.\n", (unsigned long)n);
    return 0;
}
