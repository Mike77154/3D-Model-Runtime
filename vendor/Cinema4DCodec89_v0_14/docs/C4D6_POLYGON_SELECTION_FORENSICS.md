# Cinema4DCodec89 v0.8 - Native Tpolygonselection ranges

## Confirmed binary observations

In XC4DC4D6 files from three game corpora, records that belong to native
Tpolygonselection tags use class ID 0x1629 (5673 decimal) and a typed
UTF-16BE name (`0x82`, BE32 byte length, 00 ASCII char repeated).

For a compatible observed record, the polygon interval is framed as:

```hex
0C 00 02 01 00 01 AD BB 00 00 00 00
0F 00 00 00 01
0F XX XX XX XX     ; first polygon index (BE32)
0F YY YY YY YY     ; final polygon index (BE32, inclusive)
```

The range is **not** vertex data, UV data, a material pointer, nor a bone weight.
Parser validates the class, name, marker, ordering, and final index
against the native mesh face count. A table can contain zero or multiple
ranges with same selection name; do not assume 1-based selection names
map to source material indices.

### Reproduced offsets (C4D6 samples)

FemaleHobo.c4d:
- `Selection 2`: marker 0x61D45, start 0x0894=2196, end 0x0F8F=3983
- `Selection 1`: marker 0x62000, start 0, end 0x0893=2195
- 3,984/3,984 polygons covered; no overlap.

MalePunk.c4d:
- `Selection 2`: marker 0x5E41D, start 0, end 959
- `Selection 1`: marker 0x5E6D8, start 960, end 3653
- 3,654/3,654 polygons covered; no overlap.

EGF2.c4d (Darkside Chronicles): nine selections together cover
12,707/12,707 polygons. The names are not necessarily in ascending
face-index order.

## Important distinction: selection != material binding

A polygon selection is a set of face IDs. The physical references linking
`TextureTag`, selection and a particular material bitmap have not been fully
reconstructed from this proprietary serialization. A visually convincing
mapping for two models is **a validated preview hypothesis**, not proof of
universal `TextureTag -> Material -> JPEG` resolution.

The library therefore only emits selection ranges and groups. It never
silently invents a material ID. Engine callers can attach a material explicitly
once they have a verified binding.

## UV / vertical origin

Native per-corner UV coordinates need to be interpreted against an image
with **top-left** pixel addressing (V increases downward). The diagnostic
preview uses `pixel_y = V * image_height` and showed correct alignment.
Standard OBJ consumers conventionally address textures using bottom-left V,
therefore `c4d89_mesh_obj.c` exports `V_obj = 1 - V_native`. **Do not flip V
again in a renderer that already understands the OBJ convention.**

## Audit (complete set of 260 original ZIP/RAR members, duplicate basenames kept)

- 225/260 files: at least one recognized Tpolygonselection range.
- 220/260 files: all mesh polygons covered with no overlap.
- 5/260: recovered selections but incomplete coverage (Umbrella Chronicles).
- 35/260: no recoverable selection ranges using this exact record layout.
- 0/260 CLI failures.

The 35 unmatched scenes remain supported by older geometry recovery;
absence of recovered selection does not mean the model is damaged.
We do **not** claim material bindings are recovered for all 260 scenes.

## C89 public functions

- `c4d89_scan_poly_regions(path, mesh_table, regions)` - native byte scan.
- `c4d89_find_face_regions(...)` - face-to-selection membership.
- `c4d89_export_obj_groups(path,dest)` - diagnostic OBJ with `g` groups
  (no unverified `usemtl` output).

All new C code is C89 strict; static buffers, no `malloc`, no `free`,
no floating point, no uint64 or SDK dependency.

## Future forensic targets

- Native `TextureTag` selection restriction reference.
- Native `TextureTag` material link and material->shader->file link.
- Cases with non-contiguous/missing selection streams.
- Multiple meshes and multiple UVW tags with different projections.
