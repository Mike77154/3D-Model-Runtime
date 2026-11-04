# API rápida

## Core

```c
void grc_options_default(grc_options *opt);
void grc_model_init(grc_model *model);
void grc_model_free(grc_model *model);
int grc_load_model(const char *path, grc_model *out_model, const grc_options *opt);
int grc_save_model(const char *path, const grc_model *model, const grc_options *opt);
```

## Análisis

```c
int grc_model_analyze(const grc_model *model, grc_model_stats *out_stats);
int grc_model_validate(const grc_model *model, grc_model_stats *out_stats, char *report, size_t report_size);
void grc_model_compute_bounds(const grc_model *model, grc_vec3 *min_out, grc_vec3 *max_out);
```

## Limpieza / preparación

```c
int grc_model_prepare(grc_model *model, const grc_options *opt);
int grc_model_drop_invalid_faces(grc_model *model, grc_u32 drop_flags);
int grc_model_triangulate_quads(grc_model *model);
int grc_model_generate_flat_normals(grc_model *model);
int grc_model_remove_unused_vertices(grc_model *model);
int grc_model_weld_vertices(grc_model *model, float epsilon);
void grc_model_flip_v(grc_model *model);
```

## Flags de preparación

```c
GRC_CLEAN_DROP_INVALID
GRC_CLEAN_DROP_DEGENERATE
GRC_CLEAN_TRIANGULATE_QUADS
GRC_CLEAN_GENERATE_NORMALS
GRC_CLEAN_REMOVE_UNUSED_VERTICES
GRC_CLEAN_WELD_VERTICES
GRC_CLEAN_PSX_QUANTIZE
```

## Formatos directos

```c
int grc_load_gfm(...); int grc_save_gfm(...);
int grc_load_obj(...); int grc_save_obj(...);
int grc_load_tmd(...); int grc_save_tmd(...);
int grc_load_emd(...);
int grc_load_md1(...); int grc_save_md1(...);
int grc_load_md2(...); int grc_save_md2(...);
```
