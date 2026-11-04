# hl2modelib 1.0.5 — vendored master + runtime modules

## Layout

```text
include/hl2modelib.h                existing types, unchanged ABI
include/hl2m_master89.h              master ABI 0x0105
vendors/source/master/               master codec / public routing
vendors/source/formats/              MDL, VVD, VTX, VMT, PHY, ANI, SMD, VTA, QC
    mdl_codec89.inc                  private parser implementation
    vvd_codec89.inc                  private parser implementation
    vtx_codec89.inc                  private parser implementation
    vmt_codec89.inc                  private parser implementation
    phy_codec89.inc                  private parser implementation
    ani_codec89.inc                  private parser implementation
    smd_codec89.inc                  extracted decoder + serializer
    vta_codec89.inc                  extracted decoder + serializer
    qc_codec89.inc                   extracted decoder + serializer / IR
    {smd,vta,qc,ani}_vendor89.c       separately linkable format interfaces
vendors/source/mesher/               mesh assembly adapter
vendors/source/texture/              native VTF decode
vendors/source/animator/             native MDL animation sampling
vendors/source/texturizer/           VMT -> effect description ONLY
vendors/source/shaderer/             fragment shader implementation ONLY
vendors/source/physics/              PHY summary, AABB and ground stepping
vendors/source/ik/                   QC IK metadata bridge, contact root correction
```

## Source format routing

- `hl2m_master_decode89(HL2M_FORMAT_SMD, src, len, 0, 0, &smd)`
- `hl2m_master_decode89(HL2M_FORMAT_VTA, src, len, 0, 0, &vta)`
- `hl2m_master_decode89(HL2M_FORMAT_QC, src, len, 0, 0, &qc)`
- `hl2m_master_decode89(HL2M_FORMAT_ANI, src, len, &mdl, 0, &ani)`

For `.ani`, first decode its matching MDL (and verify checksums). Keep
`SMD/QC/VTA` outputs in static storage rather than the stack because fixed
arrays in these public types are large.

**Important internal boundary:** each parser is in its own `.inc` file and
exposed through a separately compiled `{format}_vendor89.c` facade; the parser
implementations continue to be compiled in `src/hl2modelib.c` because they
reuse *private* C89 lexer/byte-reader helpers. This is not yet full standalone
linkage of each underlying parser. It preserves every previous exported symbol
and the old compilation pipeline without duplicating definitions.

## Physics (fixed 16.16)

- `hl2m_master_phy_summary89(phy, summary)`: metadata/checksum and compact vs
  legacy solid header count.
- `hl2m_master_physics_step89(body, dt, gravity_z, ground_z)`: simple
  deterministic velocity integration / flat-ground collision. `dt` 0..1 in
  16.16; velocity and gravity clamped to an explicitly supported range.
- `hl2m_master_physics_overlap89(a, b, &overlap)`: AABB overlap query.

The `.phy` parser provides header-level solid information, **not actual VPhysics
convex hull vertices or per-triangle collision**. No rigid body contacts,
constraint solver, ragdoll, cloth, or VPhysics exact simulation is claimed.

## IK (fixed 16.16)

- `hl2m_master_ik_qc89(...)`: forwards existing QC IK rule metadata to the
  legacy rule bridge (which remains approximate).
- `hl2m_master_ik_root_plant89(...)`: contact point correction in world space
  for one named bone index, bounded by a per-axis max shift. Uses the actual
  current `world` matrix translation; does not change bone rotations.
- `hl2m_master_ik_apply_root89(...)`: apply that entity/root correction to
  all pose world transforms and local root transforms, without breaking
  skeleton segment lengths. Caller then skins using the corrected matrices.

This is useful for simple foot pinning with physics ground position and as an
engine API bridge. **It is NOT two-bone ankle/knee IK**, and cannot yet account
for alternating foot contacts or full animation sequence IK in Source.

## Shaderer (fixed 16.16)

- `hl2m_master_material89()` decodes VMT and extracts normal/Phong/rim flags
  via the texturizer.
- `hl2m_master_shade89(&fx, &fragment, rgba)` runs the detached software
  fragment shader on a sample provided by the presenter. No SDL/OpenGL/D3D
  linkage required. It is **not the original Valve GPU shader**. Tangent frame
  and textures must be provided by the host.

## Build and regression

```sh
make clean
make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
make check
./test_vendors105
sh tests/test_t103_corpus.sh /path/to/t103 /tmp/hl2m_t103_export
```

The supplied T-103 corpus exercised the seven complete MDL/VVD/VTX triplets
without touching vertex or material decoding. This release reorganizes source
and adds isolated helper APIs; **it does not claim animation blending, GPU
shader parity, full PHY physics or full IK**.

## Versioning

Legacy `hl2modelib.h` functions remain public and unchanged. The master now
advertises `HL2M_MASTER_ABI89 == 0x0105`. All modules use caller-owned memory;
no `malloc`, `free`, `float`, `double`, or 64-bit arithmetic.
