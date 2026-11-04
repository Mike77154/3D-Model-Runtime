# Modular C89 v3.6

Five independently compiled modules added to the original codec without deleting its existing components.

- animer: sequence cursor and keyframe selection adapter (no quaternion interpolation yet).
- posefeamw: Q16.16 vector interpolation primitive (not full skeletal pose evaluation).
- uv_texturer: UV retrieval from binary32 payload via integer fixed conversion.
- mesher: vertex and UV extraction for standard mesh payloads.
- texturer_opener: bounded DDS/BMP/PNG header probes only. **NOT image decoders**. Compressed DDS and PNG pixels are not decoded in this version.

No CPU skinning, inverse bind matrices, quaternion reconstruction, material rasterization or rendered animation is claimed. Modules use caller-owned outputs, no heap, no float/double, no stdin, no long long.
