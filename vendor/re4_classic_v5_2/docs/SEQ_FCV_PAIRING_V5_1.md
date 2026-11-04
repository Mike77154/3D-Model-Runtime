# RE4 Classic Codec v5.1 — Evidence-based FCV/SEQ pair checking

## Why

v5.0 can dispatch raw SEQ codes and key times but cannot establish what FCV
clip owns a given SEQ. v5.1 adds an **explicit, reversible** pair inspector and
a provenance guard. This is not reverse engineering of the event handler.

The observed PC 2007 time relation is:

```
FCV.max_time         : integer final frame (14-bit header field)
SEQ.key.frame_q6     : event position in 10.6 units (64 units / frame)
FCV final position   : FCV.max_time * 64
next cycle begins    : (FCV.max_time + 1) * 64
```

That last number is a **candidate playback period**, not a verified assertion
about all original engine modes. It is used only for explicitly authorized pair
playback. The existing dispatcher supports only periods <=65536 Q6; longer
FCV durations fail closed rather than wrapping.

## Native C89 changes

- `animation/re4_seq_pair.c`, `include/re4c3d/re4_seq_pair.h`:
  - `re4_seq_pair_probe()` parses both full resources using real format readers,
    checks their internal integrity, reports whether the last SEQ key equals,
    precedes or exceeds the FCV last frame.
  - Fingerprints are FNV-1a 32-bit checksums: useful for accidental mismatch
    detection and reproducibility, **not cryptographic security**.
  - `re4_seq_pair_dispatch()` verifies *both* caller-provided fingerprints,
    refuses overruns and overlong clips, and requires explicit opt-in for an
    early-ending SEQ. It then delegates to the v5.0 deterministic dispatcher.
  - No automatic association based on file order, names, or time coincidence.
  - Uses fixed-point integers, caller-owned arena, zero heap, no C99 runtime.
- `examples/re4_seq_pair_probe.c` to check two explicitly supplied files.
- `tests/re4_seq_pair_test.c` tests valid exact pairs, early-ending SEQ,
  overrun, long clips, mutated checksums and two-loop playback with synthetic
  data, shipping no copyrighted game assets.

## Input examples

```sh
make -j4 re4_seq_pair_probe re4_seq_pair_test
./re4_seq_pair_test
./re4_seq_pair_probe chosen.fcv candidate.seq
```

### Offline corpus audit (Python external tool)

```sh
python3 examples/re4_v51_pair_candidates.py --em /path/to/em.dat --out /tmp/re4-pairs
python3 examples/re4_v51_c89_crosscheck.py \
  --em /path/to/em.dat \
  --candidates /tmp/re4-pairs/SEQ_FCV_PAIR_CANDIDATES.csv \
  --probe /path/to/re4_seq_pair_probe \
  --gca /path/to/re4_gca_catalog \
  --out /tmp/re4-pairs/C89_REAL_FAMILY_SMOKE.csv
```

## Actual file results, 2026-11-02

176 nested `em\\*.dat` inspected from the user's original `em.dat`, with
12,289 SEQ entries and independent family-level matching against FCV final
frame times. A match must be understood as **timing evidence only**:

| SEQ timing category | Count |
|---|---:|
| Exactly one FCV duration matches last SEQ time | 3,403 |
| Multiple FCVs have an identical end time | 5,503 |
| No exact match, at least one FCV long enough | 3,383 |
| Total | 12,289 |

For 6,404 SEQ the immediately preceding archive entry is an FCV with an
exact matching duration, but **4,193 of those are not unique within the
family**, so adjacency must not silently override ambiguity. Specifically,
2,211 have both adjacent/exact and uniquely matching duration. Even those
are *candidates*, not verified canonical pairings.

The C89 pair probe was independently exercised on 148 unique-duration
candidates drawn from 148 different families (one per family); all 148
returned `EXACT_END_TIMING_ONLY`. No clips were automatically linked.

Examples from actual original files (index within nested DAT):

| Family | FCV | SEQ | FCV last frame | SEQ last Q6 | Result |
|---|---:|---:|---:|---:|---|
| `em35` Méndez | 19 | 20 | 148 | 9,472 | Exact timing |
| `em3c` Armadura | 25 | 26 | 129 | 8,256 | Exact timing |
| `em23` Cuervo | 7 | 24 | 18 | 1,152 | Exact timing; 0 nonzero codes |
| `em35` alternate example | 19 | 22 | 148 | 7,168 | Ends early; not auto-approved |

> Important: identical final time is not proof of original engine selection,
> SEQ index assignment, event semantics, corresponding model variant, or clip
> motion fidelity. The user did not provide a verified RE4 original PC EXE
> in this corpus for analyzing event handlers. Zero automatic canonical
> associations are asserted by this release.

## Outstanding work

1. Verify FCV/SEQ selection from executable or independently authoritative
   motion-lookup tables, rather than guesses from time or adjacency.
2. Reverse engineer native event codes in relation to Mendez phase changes,
   Plaga head transformations and Bella Sisters' chainsaws; current raw
   codes have deliberately **no assigned gameplay meaning**.
3. Extend event scheduling for long clips and native reverse/seek/IK paths.
4. Integrate with the Win32 model runtime; Win32 build not yet tested.
