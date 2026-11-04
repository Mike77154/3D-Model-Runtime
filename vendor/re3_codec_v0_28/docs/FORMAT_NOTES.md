# RE3 EMD/TIM format notes recovered with v0.13

## EMD directory

RE3 EMD uses an 8-byte header followed by a 15-offset directory at the declared
index offset. This codec preserves every section and exposes bounded raw reads.

Section indices used by this runtime:

0 Binary00 / binding
1 Binary01 / morph when validated
2 EDD A
3 EMR A
4 EDD B
5 EMR B
6 EDD Damage
7 EMR Damage
8 opaque BinaryWeapon/unknown block
9 EDD Weapon/AUX A
10 EMR Weapon/AUX A
11 EDD Weapon/AUX B
12 EMR Weapon/AUX B
13 MD2 auxiliary mesh
14 MD2 body mesh

## MD2

Object descriptors are 24 bytes. Vertex/normal records are 8 bytes. Triangle
records are 12 bytes and quad records are 16 bytes. UV/TPAGE/CLUT values are
preserved for TIM sampling.

## EDD / EMR

EDD animation entries identify clip frame lists. RE3 EMR key frames store a
speed vector, frame/root Y information, and packed 12-bit XYZ rotations.
NormalEx/Damage banks may contain frame data whose hierarchy is inherited only
when the recovered v0.13 rules can prove the joint counts match.

## Binary00 binding

The common header contains `pBindPose` at +4 and `limb_count` at +6. Each limb
index entry is 8 bytes. A/B offsets are relative to the beginning of that limb
index entry.

Ownership stream:

- `u8 joint_id`, terminated by `0xFF`
- `u8 triplet_count`
- `triplet_count * 3` vertex IDs; `0xFF` may pad the final triplet

Seam records are fixed eight-byte records:

`joint0 joint1 vertex0 blend0 vertex1 blend1 vertex2 blend2`

followed by a single `0xFF` terminator. Blend is interpreted as Q0.7 where
`weight1 = blend / 128` and `weight0 = 1 - weight1`.

## Binary01 morph

Recovered retail grammar:

- `u32 part_mask`
- `u32 records_end`
- `u32 rest_pos_size`
- one track record for each set bit in ascending part index:
  - `u32 vertex_data_offset`
  - `u32 frame_stride`
  - `u32 extra_frame_count`
  - `u32 reserved[extra_frame_count]`
- rest-pose copy for Normal joints, packed signed XYZ, aligned to four
- morph frames for selected mesh parts

The track stride is `align4(vertex_count * 6)`. Frame count is
`extra_frame_count + 1`.

## Section 8

v0.13 deliberately keeps section 8 opaque. In the preserved 100-EMD retail
corpus, 74 files had a four-byte zero block and 26 had a 20-byte block split
between two repeated signatures. No semantic structure is asserted without
additional proof.

## TIM

TIM magic is 0x10. The codec loads CLUT and pixel rectangles into a fixed PSX
VRAM image and resolves MD2 UVs through TPAGE/CLUT. `0x0000` maps to transparent;
`0x8000` is retained as opaque black by the simple ARGB conversion rather than
being incorrectly discarded as transparent.
