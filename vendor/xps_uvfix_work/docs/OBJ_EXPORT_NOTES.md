# OBJ export notes: XPS UV V origin

The XPS model callbacks keep UV values exactly as they are stored in the XPS stream.
For Wavefront OBJ export, the V component must be emitted as `1 - raw_xps_v`.

Reason:

- XPS/XNALara-style texture data in this test asset uses the opposite V origin from the OBJ reference export.
- The geometry and normals match the reference OBJ, but the reference `vt` values are `u, 1-v` compared with the raw XPS callback data.
- Do not clamp during export. OBJ/MTL default texture behavior is repeat when clamp is off; tiny negative UV values from seams should remain as authored.

The examples now do this:

```c
#define EXAMPLE_FX_ONE ((xps_fx)65536L)
obj_v = (xps_fx)(EXAMPLE_FX_ONE - raw_xps_v);
```

This keeps the core importer lossless/raw while making the OBJ bridge visually match the reference export.
