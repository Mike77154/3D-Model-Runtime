# FBX skeleton + texture fix

This revision fixes the first model_runtime89/model_viewer_w32gl89 bundle after real FBX testing.

## Runtime

- Recognizes FBX `Model` nodes typed `LimbNode`, `Root`, or `Skeleton` as native bone nodes even without skin clusters.
- Resolves bone parent indices only after all bone IDs exist, so FBX serialization order cannot erase the hierarchy.
- Preserves cluster skin weights and evaluates skinned FBX vertex positions in the provider visit path when bind data is available.
- Adds `modelrt_texture_encoded_view_get()` so a provider can expose borrowed encoded media bytes without creating a universal texture object.
- FBX provider exposes embedded media through that borrowed view and keeps external texture paths as native metadata.
- Includes `tests/fbx_regression.c`, `rigged_test.fbx`, and `bones_only_test.fbx`.

## Viewer integration expectation

The Win32 viewer consumes the texture metadata/blob API. Image decoding remains a viewer/backend concern rather than a model-runtime conversion step.
