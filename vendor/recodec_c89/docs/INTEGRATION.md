# Integración en engine/editor/visor

## Carga básica

```c
grc_model m;
grc_options opt;

grc_options_default(&opt);
grc_model_init(&m);
if (grc_load_model("player.pld", &m, &opt) == GRC_OK) {
    /* Subir m.vertices, m.uvs, m.faces a tu renderer */
}
grc_model_free(&m);
```

## Validación antes de meter a renderer

```c
grc_model_stats st;
char report[1024];

grc_model_validate(&m, &st, report, sizeof(report));
puts(report);
```

## Preparación para exportar a PS1/RE

```c
opt.flags = GRC_CLEAN_DROP_INVALID |
            GRC_CLEAN_DROP_DEGENERATE |
            GRC_CLEAN_REMOVE_UNUSED_VERTICES |
            GRC_CLEAN_GENERATE_NORMALS |
            GRC_CLEAN_PSX_QUANTIZE;

grc_model_prepare(&m, &opt);
```

## Export nativo

```c
opt.native_game_hint = GRC_GAME_RE2;
grc_save_model("EMY00.EMD", &m, &opt);

opt.native_game_hint = GRC_GAME_RE3;
grc_save_model("enemy_re3.emd", &m, &opt);
```

## Uso recomendado del cache

```text
original RE file / authoring OBJ
        ↓
grc_load_model()
        ↓
validar + limpiar
        ↓
guardar .gfm como cache/editor project
        ↓
exportar OBJ para DCC o nativo para tooling
```

## Piezas/limbs

`m.pieces` permite listar rangos lógicos:

```c
for (i = 0; i < m.piece_count; ++i) {
    printf("%s faces=%u\n", m.pieces[i].name, m.pieces[i].face_count);
}
```

En OBJ, esto sale de `o`/`g`. En TMD/MD/EMD, sale de objetos/descriptores/limbs cuando el parser puede inferirlos.
