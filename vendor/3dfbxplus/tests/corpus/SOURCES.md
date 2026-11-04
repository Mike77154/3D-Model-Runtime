# Suggested external FBX corpus sources

The codec tree does not bundle third-party FBX files.  Use this list as a local validation checklist and keep licenses/manifests beside downloaded assets.

## Small/public sample repositories

- `kenshaw/fbxtest` — compact test FBX set with converted variants.
- `ladybug-tools/3d-models` — MIT-licensed AEC-oriented 3D model repository.
- `mixandjam/Jammo-Character` — free character model with Mixamo-oriented rig workflow.

Use:

```sh
./tests/fetch_public_corpus.sh
FBX_CORPUS_DIRS="$PWD/tests/public_corpus/fbxtest:$PWD/tests/public_corpus/ladybug_3d_models:$PWD/tests/public_corpus/jammo_character" \
BUILD_DIR=$PWD/build ./tests/run_real_corpus.sh
```

## Larger/reference-style corpora

- ufbx public dataset: useful for high-coverage differential validation.
- CMU motion capture FBX datasets: useful for animation/skin stress tests.
- Exporter-generated matrices from Blender, Maya, 3ds Max, Cinema 4D, MotionBuilder, and Mixamo.

For private/commercial tools, keep only generated manifest + hashes in this repository unless redistribution rights are explicit.
