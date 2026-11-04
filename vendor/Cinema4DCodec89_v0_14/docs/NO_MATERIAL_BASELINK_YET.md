# Exact TextureTag → Material link: unresolved

The native tags contain selection names and their polygon coverage, but the material pointer serialized in BaseLink/1010 has not been traced to a specific Mmaterial instance. The material bitmap scanner preserves `material_offset` for multiple shaders within a material. These offsets can be compared, but neither similar order nor nearby object locations prove the BaseLink. **No automatic material-to-polygon assignment is made by the new resolver.**

High-priority reverse-engineering plan:
1. Isolate a tiny `.c4d` with two materials and two TextureTags, save a variant with assignments swapped, and binary-diff the saved tag records while holding names/geometry constant.
2. Identify 1010's typed BaseLink serialization and target material identity across both saves.
3. Validate the decoded link on the 260 scene corpus, including models where tag order and bitmap record order differ.
4. Add per-channel shader decoding (diffuse/normal/alpha/bump) and compare renderer output with Cinema 4D reference screenshots.

Without paired reference scenes, we cannot currently prove that last edge. The v0.11 1,823/1,901 figure is file-resolution coverage, not fully correct UV/material coverage.
