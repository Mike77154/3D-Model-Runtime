# Phase 02 modular vendor map

```text
BLEND raw file
   |-- header decoder 12-byte legacy / 17-byte extended
   |-- block scanner BHead4 / SmallBHead8 / LargeBHead8
   |-- SDNA NAME, TYPE, TLEN, STRC
   |-- serialized-pointer resolver (two 32-bit limbs, interior references)
   |-- Q16.16 decoder
   +-- mesh/
       |-- legacy (MVert, MFace)
       |-- transitional (MPoly, MLoop)
       |-- CustomData `position`
       |-- modern AttributeStorage arrays
       |-- modern face_offset_indices + corner_vert
       +-- UV vendor (CD_MLOOPUV / CD_PROP_FLOAT2)
   +-- obj/ multi-mesh output + indices (implemented in mesh legacy frontend)
   +-- forthcoming vendors (not implemented):
       |-- armature/ weights/ skin/ morph/ shapekey/
       |-- actions/ fcurves/ nla/ ipo/
       |-- object/ scene/ parenting/ transforms/
       |-- materials/ images/ texture/ packed/
       |-- curves/ hair/ geometrynodes/
       +-- compression/ gzip/ zstd/
```

**Policy:** never map serialized bytes directly to host structs. All serializers are version-dependent and may store 32/64-bit pointer tokens irrespective of the host machine architecture. Module interfaces use static views `{offset,size}` and fixed-point decoded coordinates. One block can contain many structs and pointers may reference a subrange of that block. No fixed geometry or animation compatibility claims without Blender-authored corpus.
