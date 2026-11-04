# C4D6 texture mapping fix (v0.7)

## Summary
This revision corrects the OBJ/preview texture placement path by flipping the exported V coordinate:

- previous export: `vt u v`
- current export: `vt u (1 - v)`

## Why
The native UV records recovered from the XC4DC4D6 files behave as top-left oriented in the current evidence set, while the downstream OBJ tooling and preview path expect the usual bottom-left style V axis.

This mismatch produced textures that appeared vertically displaced or incorrectly seated on the mesh in the diagnostic previews.

## Scope
- Fix applies to the OBJ export / preview path.
- Native UV recovery itself is unchanged.
- Exact submaterial assignment remains partially heuristic until polygon/material tags are fully decoded.

## Practical result
- FemaleHobo preview aligns better with the embedded JPEG atlases.
- Turntable previews preserve perspective and improve texture placement.
