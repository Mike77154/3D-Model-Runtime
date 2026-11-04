# Real FBX corpus drop-zone

Put externally sourced FBX files here for local validation:

```txt
corpus/
├─ blender/
├─ maya/
├─ 3dsmax/
└─ mixamo/
```

The repository does not bundle third-party assets. Run:

```sh
BUILD_DIR=$PWD/build ./tests/run_real_corpus.sh
```

or point at external directories:

```sh
FBX_CORPUS_DIRS="/assets/blender:/assets/maya:/assets/max:/assets/mixamo" \
BUILD_DIR=$PWD/build ./tests/run_real_corpus.sh
```


## Optional public corpus fetch helper

A helper is included for local-only downloads of public FBX sample repositories:

```sh
./tests/fetch_public_corpus.sh
```

Then run:

```sh
FBX_CORPUS_DIRS="$PWD/tests/public_corpus/fbxtest:$PWD/tests/public_corpus/ladybug_3d_models:$PWD/tests/public_corpus/jammo_character" BUILD_DIR=$PWD/build ./tests/run_real_corpus.sh
```

The helper deliberately does **not** bundle third-party assets into this source tree. Audit upstream licenses before redistributing any downloaded FBX files.
