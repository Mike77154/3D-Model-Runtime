# Giffany v9 — MaxPush Geometry + TKEV runtime

Este pase empuja otra capa de compatibilidad práctica contra Genesis3D original, especialmente alrededor de `.bdy` y eventos de `.mot`.

## Cambios fuertes

- `g3d_body_parse_geometry()` ahora es API pública para leer directamente el payload crudo `Geometry`/`BODY`, el mismo subarchivo que `geBody_WriteGeometry()` mete dentro del VFS `.bdy`.
- `g3dtool` detecta el tipo `BDY-GEOMETRY` y lo acepta en `verify`, `info`, `dump`, `canon`, `pack` y `makecf`.
- `g3dtool extract` ahora saca:
  - `body.bdy`
  - `geometry.body`
  - `bitmap_###.bin` para payloads `Bitmaps/N`
  - motions `###_name.mot`
  - `manifest.txt`
- Eventos `TKEB/TKEV` quedaron expuestos como runtime:
  - `g3d_tkevents_get_text()`
  - `g3d_tkevents_count_between_fix16()`
  - `g3d_tkevents_find_next_between_fix16()`
  - `g3d_motion_count_events_between_fix16()`
  - `g3d_motion_find_next_event_between_fix16()`
- CLI nuevo:

```sh
./g3dtool events walk.mot 0.0 1.0
./g3dtool events player.act 0.0 1.0 0
```

La semántica del rango sigue la del `geTKEvents_GetExtents` / iteración práctica del engine: evento incluido si `start <= t < end`.

## Endurecimiento

- `g3d_motion_parse_binary()` ya no ignora bytes sobrantes: si quedan trailing bytes después de un `MTNB/MOTN` válido, devuelve `G3D_ERR_CORRUPT`.
- `selftest` cubre ese caso con un motion binario válido + un byte basura.

## Regresiones nuevas

- Raw `Geometry/BODY` round-trip con `g3d_body_write_geometry()` + `g3d_body_parse_geometry()`.
- `.bdy` con `Bitmaps/0` real, incluyendo parse normal y parse CF00.
- `extract` exporta `bitmap_000.bin`.
- `canon` convierte `geometry.body` crudo a `.bdy` VFS canónico.
- `pack` puede reconstruir un actor desde `geometry.body`.
- `events` lista `TKEV` ASCII y eventos conservados tras canonicalizar a binario.

## Nota honesta

`Geometry/BODY` crudo no contiene los bytes de `Bitmaps/N`; solo conserva el flag de material. Si canonicalizas o empaquetas desde `geometry.body` directamente, el `.bdy` resultante puede tener entradas de bitmap vacías. Para preservar texturas embebidas, usa `body.bdy` completo o gestiona `bitmap_###.bin` aparte.
