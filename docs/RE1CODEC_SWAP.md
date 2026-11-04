# RE1codec v0.3-mvref replacement

`model_runtime89` vendors `vendor/re1codec_v0_3_mvref` and no longer contains
the previous RE1codec v0.1 adapter/vendor. The stable provider ID remains
`MODELRT_CODEC_RE`.

Supported native model containers:

- EMD: optional bank A + bank B + TMD + optional embedded TIM
- EMW: bank B + TMD; no embedded TIM
- standalone TMD: geometry only

The provider uses `re1c_open_model()`, `re1c_bank_available()` and
`re1c_get_preferred_bank()`. It never synthesizes a missing animation bank.

## Hierarchy semantics

RE1 animation is rigid-part animation. The provider exposes
`MODELRT_CAP_RIGID_HIERARCHY` and `modelrt_rigid_part_view`; it does not expose
`MODELRT_CAP_SKELETON` or `MODELRT_CAP_SKINNING`. Parent relationships come
only from EMR PartList. TMD object count remains independent from EMR part
count, as required by v0.3 and the RE1MV reference corpus.

## Animation timing

EDD `unknown_word` is preserved as opaque metadata by the codec and is not
used as frame duration. Clips run at the RE1MV-confirmed 40 ms per frame ref
(25 FPS).

## Geometry

Both FT3 and FT4 packets are accepted. FT4 remains a native four-sided packet
in RE1codec and is emitted to the generic triangle visitor as two transient
triangles.

## Textures

Embedded EMD TIM is sampled natively by CBA/TSB. EMW/TMD have no embedded TIM
or filename; the runtime exposes unresolved external texture slots so a host
may bind a texture without inventing source metadata.
