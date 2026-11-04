# StudioHalfLife Ultra source scan notes

Local sources inspected from the uploaded code bundles:

- `xash3d-fwgs-master/engine/studio.h`
  - `studiohdr_t::studiohdr2index`
  - `studiohdr2_t` pose parameters, IK autoplay locks, IK chains, keyvalues, hitbox sets
  - `mstudioseqdesc_t` autolayer/iklock/keyvalue fields and blend group extensions
  - `mstudioikrule_t`, `mstudioikchain_t`, `mstudioiklink_t`, `mstudioiklock_t`
  - `mstudioboneweight_t { uint8 weight[4]; int8 bone[4]; }`

- `xash3d-fwgs-master/ref/gl/gl_studio.c`
  - Xash path checks `STUDIO_HAS_BONEWEIGHTS`
  - `blendvertinfoindex` and `blendnorminfoindex` are read from the reused model fields
  - vertex skinning uses computed skin matrices for weighted vertices/normals

- `xash3d-fwgs-master/utils/mdldec/qc.c` and `utils/mdldec/smd.c`
  - decompiler emits/handles Xash boneweights and strips unsupported flags from classic QC path

Ultra additions are intentionally separated from the parser core so classic MDL v10 behavior remains stable.
