# RE4 Classic Codec v5.0 — SEQ raw event dispatcher

## Purpose

A C89, caller-owned, heap-free SEQ event scheduler using original PC 2007
raw event keys `{code:u8, auxiliary:u8, frame_q6:u16}`. It resolves **when**
to emit a raw event across frame ranges, loop boundaries, and ties. It does
not identify **what** each code means to the original game and it does not
infer a SEQ–FCV association from archive adjacency.

`re4_seq_dispatch_range(seq,duration_q6,from_q6,to_q6,max_loops,
include_zero,callback,user,&count)` processes the half-open interval
`[from_q6,to_q6)`. All times are integer Q6, so **64 units = one frame**.
`from_q6` and `to_q6` are unwrapped unsigned 32-bit values, while SEQ keys
are 16-bit. `clip_duration_q6` must be 1..65536 and greater than the
last SEQ key; it must be set from a **verified clip relationship**, not
assumed from the last SEQ key. The QA auditor deliberately sets a synthetic
period of `last_q6+1` only to test scheduler invariants, **not** to claim
original playback speed.

- Stable key order, including multiple codes at the same time.
- No duplicate event at split half-open window boundaries.
- Correct event repetition when the timeline loops.
- Rejects malformed time order, invalid duration and too many loop cycles.
- `include_zero=0` suppresses zero-coded keys; `include_zero=1` emits *all*
  keys, including zero, since zero semantics are not yet verified.
- Callback errors return immediately with the number successfully emitted.
  Dispatch is streaming, not transactional.
- No `malloc/calloc/realloc/free`, no float/double, no heap.

### CLI

```sh
make -j4 re4_seq_replay re4_seq_replay_audit re4_seq_dispatch_test
./re4_seq_dispatch_test
./re4_seq_replay em35.dat 20 9536 0 19072
./re4_seq_replay_audit em35.dat > em35_replay.csv
```

`9536` is an **explicit test duration** in Q6 units (149 frames), used to
replay SEQ 20 of the supplied `em35.dat` twice. Archive proximity to FCV 19
has been observed, but it is not proof of canonical attachment.

### Full corpus external QA

```sh
python3 examples/re4_v50_replay_corpus.py --em /path/to/em.dat --out /tmp/re4-v50
python3 examples/re4_v50_seq_codebook.py --em /path/to/em.dat --out /tmp/re4-v50
```

This real-file regression, using the local `em.dat` available during v5.0,
reported:

| Metric | Observed |
| --- | ---: |
| Nested `em` DAT packages scanned | 176 |
| Packages with one or more SEQ | 166 |
| SEQ replay invariants passed | 12,289 / 12,289 |
| Raw SEQ keys examined | 841,708 |
| Keys with code other than zero | 183,621 |
| Distinct byte codes, including zero | 50 |
| Replay mismatches found | 0 |

Each SEQ was replayed in one pass, in two half-intervals, and in the next
loop; sequence order and counts were compared. This is **not** validation of
Capcom's original event-handler semantics, weapon events, morphs, IK, or
transformation triggers. Runtime integration still needs a proven SEQ–FCV
binding table and a reverse-engineered event handler mapping.

### Priorities after v5.0

1. Verify SEQ–FCV pairing through original executable references or independent
   source data instead of assuming physical adjacency.
2. Correlate raw codes to game action handlers and compare real transformed
   enemy poses (Mendez Phase 2, Plaga head types A/B/C, Bella Sisters).
3. Add actual renderer/runtime event-handler adapters once event semantics have
   reliable evidence.
4. Verify Win32 build and playback; no such verification is claimed here.
