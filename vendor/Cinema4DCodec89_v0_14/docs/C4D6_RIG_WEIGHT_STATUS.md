# Skeleton, skin weights and texture bindings — precise current status

## Found directly in the native file

In `FemaleHobo.c4d`, the typed UTF16BE name table contains `Skin` (offset 79726), `Weight` (offset 81275), and `Joint` (offset 459167). Nearby named objects include `Pelvis` (460825), `SpineRoot` (461581), `RThigh` (462343), `RCalf`, `RFoot`, `Neck`, `Head`, `Jaw`, etc. 30 joint-region **name candidates** detected, but the parser has no reliable graph links. Naming proximity alone does not prove parent-child structure.

In one captured `Weight` payload, around offset 81500, there are dense binary tables with little-endian-looking integers, bit-packed regions and float-like values. We have **not** established their element schema or mapping to mesh vertex indices, and therefore must not use them as skin weights. Reconstructing by guess would introduce exactly the limb explosions/root errors the user wants to avoid.

## Texture resources

`FemaleHobo.c4d` has typed names `FemaleHoboHeadA.jpg` (offset 915) and `FemaleHoboBody.jpg` (offset 40070). Native JPEG blobs follow:

| Candidate | JPEG offsets | Preferred stored image |
|---|---|---|
| HeadA.jpg | 4152 (90×90), 8168 (256×256 RGB), 38673 (256×256 grayscale) | JPEG 1 |
| Body.jpg | 43305 (90×90), 47395 (256×256 RGB), 77766 (256×256 grayscale) | JPEG 4 |

JPEG association is based on the nearest preceding typed texture reference; this is **not** the same thing as decoding an exact material→polygon selection link. The 256×256 JPEGs are actual embedded scene resources, not evidence that Cinema 4D's external original textures are capped at 256 pixels.

Per-corner UVs already stream from the mesh module, but binding each texture to faces needs selection tags. The current test image merely displays retrieved originals side-by-side, without claiming texture mapping on 3D surfaces.

## Remaining reverse-engineering targets

1. Identify and validate object IDs, parent references and local matrices in the serialized graph.
2. Decode weight table vertex IDs, bone IDs and weights, using vertex_count cross-checks, sum-to-one checks and known bind pose.
3. Associate animations to exact joints, then test skinning in a rendered pose against trusted C4D sources.
4. Decode selection tags (`Selection 1`, `Selection 2`), texture assignments and corner UV set mapping.

This project's native decoder is an independent black-box format investigation. The SDK archives were used for understanding semantics, not included, copied or linked into the runtime.
