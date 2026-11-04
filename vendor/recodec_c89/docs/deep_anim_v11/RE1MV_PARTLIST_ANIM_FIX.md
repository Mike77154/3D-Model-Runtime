# RE1MV PartList animation hierarchy fix

## What was wrong

The EDD/EMR pose table was being read correctly, but the diagnostic renderer was
assembling EM1111 with the hardcoded RE1MV `Build_Model` stack.  For this zombie
that is the wrong hierarchy for a robust standalone renderer: torso, legs, and
arms can drift apart even when the frame data is correct.

The decisive clue came from RE1MV's own `EMR2ANI` export.  A `.ANI` file begins
with the EMR `PartListOffset` child table.  In `EM1111_00_idle.ANI`, that table is:

```text
00 -> 1, 9, 12
01 -> 2, 3, 6
02 ->
03 -> 4
04 -> 5
05 ->
06 -> 7
07 -> 8
08 ->
09 -> 10
10 -> 11
11 ->
12 -> 13
13 -> 14
14 ->
```

That is the actual skeleton tree for this model.  The renderer now uses this
byte table by default.

## Animation table result for EM1111 bank B

The EDD table says bank B has 31 entries indexed `0..30`.  This matches the
usual RE1MV UI wording where users say “30” because the last index is 30.

The first user-visible sequences line up as:

| clip | frames in table | note |
|---:|---:|---|
| 00 | 31 | idle / drunk sway candidate, frames 0..30 |
| 01 | 31 | stepping candidate, frames 0..30 |
| 02 | 10 | arms rising candidate, frames 0..9 |
| 03 | 42 | classic raised-arms walk candidate, frames 0..41 |
| 04 | 20 | hit reaction candidate, frames 0..19 |
| 05 | 20 | bite/headbutt candidate, frames 0..19 |

The one-frame offset versus “30/9/41/19” wording is an index/count convention:
RE1MV displays `0/30` for 31 stored frames.

## Cross-check against RE1MV-exported ANI files

`tools/re1_compare_ani.py` was added to compare the EMD's EDD/EMR data against
RE1MV `.ANI` exports.  Results:

```text
EM1111_00_idle.ANI:
  tree_match=True
  frame_count: emd_selection=31 ani=31
  max_abs_delta=0.000495
  status=OK

EM1111.ANI:
  tree_match=True
  frame_count: emd_selection=645 ani=645
  max_abs_delta=0.000495
  status=OK
```

That means the byte table, frame selection, pose indices, root values, pivots,
and angle conversion match RE1MV's exported data to within normal text rounding.

## Changed files

- `tools/emd_textured_viewer.c`
  - defaults to EMR PartList hierarchy.
  - keeps old hardcoded stack only behind `--legacy-buildmodel` / `--legacy-stack`.
  - prints `hierarchy=emr_partlist` in logs when the new path is active.
- `tools/emd_anim_table.c`
  - prints `hierarchy_partlist` and frame/pose tables.
- `tools/re1_compare_ani.py`
  - validates `.EMD` against RE1MV `.ANI` exports.
- `tools/re1_make_stable_strips.py`
  - generates strips with fixed bounds per clip.

## Useful commands

```sh
make clean && make
bash tests/smoke.sh

./build/grc validate EM1111.EMD --game re1
./build/emd_anim_table EM1111.EMD --frames
./build/emd_anim_report EM1111.EMD > EM1111_report.csv

python3 tools/re1_compare_ani.py EM1111.EMD EM1111_00_idle.ANI --bank B --clip 0
python3 tools/re1_compare_ani.py EM1111.EMD EM1111.ANI --bank B

python3 tools/re1_make_stable_strips.py EM1111.EMD docs/deep_anim_v11 \
  --viewer build/emd_textured_viewer \
  --bank B --view iso --clips 0-30 --strip-clips 0-30 --fast-atlas

# Optional: compare against the old hardcoded stack
./build/emd_textured_viewer EM1111.EMD old.bmp iso --anim --bank B --clip 0 --frame 10 --legacy-buildmodel
```

## Honest status

The animation assembly bug is fixed in the diagnostic viewer path.  The renderer
itself is still a diagnostic C tool and still uses floating point and heap-backed
frame buffers.  The no-heap/no-float cleanup has to be done as a separate pass if
this code is promoted from viewer/validator into the final runtime.
