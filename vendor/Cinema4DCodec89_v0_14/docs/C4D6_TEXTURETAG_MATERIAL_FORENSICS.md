# Cinema4DCodec89 v0.9 — Native TextureTag restrictions / material-link status

## Scope and evidence

Source: actual XC4DC4D6 files supplied in three RAR corpora. SDK used as **documentation** only.
No Maxon SDK binaries or sources incorporated in the C89 decoder.

The official SDK defines:
- `Ttexture = 5616 (0x15F0)`
- `Tpolygonselection = 5673 (0x1629)`
- `TEXTURETAG_RESTRICTION = 1006`, string referring to a polygon selection
- `TEXTURETAG_MATERIAL = 1010`, linked material

URL: https://developers.maxon.net/docs/cpp/2025_3_0/page_manual_texturetag.html

## Confirmed native bytes

Texture tag object class framing: `02 02 01 00 00 15 F0`.
Native selection tag class framing: `02 02 01 00 00 16 29`.
The texture tag payload contains a typed UTF-16BE (`82 [BE32-byte-length] ...`)
polygon restriction name *before* the following selection-tag object.
We validate the name and require an actual following polygon selection record
within 4096 bytes with an identical name. This avoids the common collision
when several meshes each contain a selection named `Selection 1`.

Observed:

| File | TextureTag byte offset | Restriction | Polygon range |
|---|---:|---|---|
| FemaleHobo | `0x61AA5` | Selection 2 | 2196..3983 |
| FemaleHobo | `0x61D60` | Selection 1 | 0..2195 |
| MalePunk | `0x5E17D` | Selection 2 | 0..959 |
| MalePunk | `0x5E438` | Selection 1 | 960..3653 |
| EGF2 | 9 tags found | 9 matching names | 9 disjoint ranges |

**Corpus audit:** 260/260 files parsed successfully; 1,487 TextureTag class records,
1,376 restriction links validated against native selection records.
220 scenes have every tag restriction verified by this method.
Unmatched tags include empty restrictions, scenes with no supported selection tag,
and alternate layouts. An unmatched record is not automatically corrupt.

## Critical distinction

The following chain is only partially resolved:

```
Ttexture 5616
  ├── restriction 1006  ---> Tpolygonselection 5673 ---> polygon IDs  [VERIFIED]
  └── material ref 1010 ---> BaseMaterial ---> bitmap shader ---> JPEG [NOT VERIFIED]
```

A visually correct two-zombie render is not evidence for the second branch.
The demonstration retains *explicit per-model* atlas bindings chosen from
image inspection. Do NOT promote tag order or texture-file order to a general
binding rule. This is a codec-quality issue: silence is preferable to
misassigning skin texture onto pants, a failure observed in MalePunk.

## Future native target

Characterize serialized `BaseLink` references for the 1010 link and trace
material ID to bitmap texture shader and file path, preferably with several
models where tag ordering and bitmap ordering differ. Check repeated `Selection`
names, overlapping tags and non-UVW projections before claiming universal mapping.
