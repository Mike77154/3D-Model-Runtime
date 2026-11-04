# T-103 XPS OBJ UV fix report

## Finding

The imported geometry and normals matched the reference OBJ, but the OBJ texture coordinate V component did not.

Raw XPS callback UV from the previous exporter:

```txt
vt 0.004791 0.157257
```

Reference OBJ export:

```txt
vt 0.004791 0.842743
```

That is exactly:

```txt
reference_v = 1 - raw_xps_v
```

## Numeric comparison after fix

After exporting OBJ with `V = 1 - raw_xps_v`:

```txt
v  maxdiff ~= 0.0000082 ; bad > 1e-4 = 0
vn maxdiff ~= 0.0000081 ; bad > 1e-4 = 0
vt maxdiff ~= 0.0000010 ; bad > 1e-4 = 0
```

Counts match:

```txt
vertices = 4674
normals  = 4674
uvs      = 4674
faces    = 7557
```

## What changed

- Kept core `xps_vertex.uv[][]` raw/lossless.
- Fixed OBJ bridge/export side only.
- `example_obj` now emits `1 - V`.
- Added `example_obj_mtl` for OBJ + MTL export with diffuse and normal map references.
- Added `docs/OBJ_EXPORT_NOTES.md`.

## Protocol

Build used:

```sh
make clean
make -j2
```

Compiler flags in Makefile:

```txt
-std=c89 -pedantic -Wall -Wextra -Werror -O2
```

No heap allocation was introduced in the added source.
