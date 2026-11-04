# FBX texture references in model_runtime89

This revision keeps the two FBX media references distinct.

`modelrt_texture_info` now exposes:

- `source_path`: the native/original media reference (`FileName`/`Filename` in FBX).
- `relative_path`: the native relative media reference (`RelativeFilename`).
- `path`: backward-compatible preferred/reference path.

The FBX codec no longer collapses `RelativeFilename` into `file_path`. The runtime
only reports what the source asset says; it does not rewrite the FBX and does not
own filesystem resolution policy.

Hosts can therefore reproduce FBX lookup rules, report stale references, or apply
their own session/project asset mapping without creating a generic model format.

Regression:

    tests/fbx_texture_path_regression.c
    tests/fbx_texture_paths.fbx

The fixture deliberately stores a different original and relative path and asserts
that both survive through `modelrt_texture_info_get()`.
