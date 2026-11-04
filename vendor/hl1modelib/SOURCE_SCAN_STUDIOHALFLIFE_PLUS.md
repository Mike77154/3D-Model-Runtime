# Source scan notes: StudioHalfLife Plus

Local source zips inspected:

- `xash3d-fwgs-master.zip`
- `reGS_hlsdk-master.zip`
- prior `studiohalflife_extensions.zip`

Useful local findings:

- Xash exposes `studiohdr2index` in the model header.
- Xash defines `STUDIO_HAS_BONEWEIGHTS` as the high flag for extended bone weights.
- Xash stores `mstudioboneweight_t` as four byte weights plus four signed bone indices.
- Xash reuses old model fields for `blendvertinfoindex` and `blendnorminfoindex`.
- Xash decompiler paths emit `$boneweights` when the model flags contain the extended boneweight path.
- Autolayers, IK chains, and IK rules are present in the Xash studio header family, but full behavioral parity remains larger than this helper layer.

Implemented this round:

- Explicit helpers around those reused fields, so the base parser can remain compatible with vanilla MDL v10.
- Weight normalization semantics that match the common renderer/decompiler pattern of compensating rounding leftovers.
- Debug renderer path that uses actual model texture/palette data instead of a single centroid color sample.
