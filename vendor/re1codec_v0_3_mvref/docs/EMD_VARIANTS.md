# RE1 EMD/EMW/TMD variants accepted by RE1codec v0.2

This note records only layouts observed in the supplied RE1 corpus. It is intentionally data-driven: the codec does not invent a humanoid skeleton or require mesh-object count to equal animation-part count.

## EMD full directory (20-byte tail)

The final five little-endian `u32` offsets delimit:

```text
0 .. off0       EMR_A
off0 .. off1    EDD_A
off1 .. off2    EMR_B
off2 .. off3    EDD_B
off3 .. off4    TMD
off4 .. dir     TIM
dir .. EOF       5 x u32 offsets
```

Observed in 34/67 supplied enemy EMD files.

## EMD B-only directory (20-byte tail)

Some models store the same 20-byte directory but omit bank A:

```text
off0 = 0
off1 = 0
0 .. off2       EMR_B
off2 .. off3    EDD_B
off3 .. off4    TMD
off4 .. dir     TIM
```

Observed in 32/67 supplied enemy EMD files. v0.1 rejected these because it required `off0 != 0`.

## EMD compact B-only directory (16-byte tail)

One supplied file (`em1032.emd`) stores only the four offsets required by its B-only layout:

```text
0                implicit missing A boundary
0                stored first u32
EMR_B end         stored second u32
EDD_B end         stored third u32
TMD end           stored fourth u32
```

v0.2 tries the normal 20-byte form first and then this compact 16-byte form.

## EMW weapon model

All 32 supplied `.EMW` files use an 8-byte tail:

```text
0 .. off0       EMR (exposed as bank B)
off0 .. off1    EDD (exposed as bank B)
off1 .. dir     standalone-header TMD
dir .. EOF       2 x u32 offsets
```

No TIM is embedded. The runtime must supply/associate the relevant external/player texture.

## Standalone TMD

The 7 supplied `WS*.TMD` files are accepted directly with `re1c_open_tmd()` or auto-detected by `re1c_open_model()`.

They contain mesh geometry only: no animation bank and no embedded TIM.

## Skeleton rule: never infer anatomy from mesh count

`TMD object_count` is a mesh/object count. `EMR object_count` is an animation-part count. They are related but not guaranteed to be identical.

In the supplied enemy corpus, bank B and TMD counts match in 46/67 files; 21/67 differ. Weapon EMW files make the distinction especially obvious: for example `W00.EMW` has a 1-object weapon mesh while its EMR bank contains 15 animation parts.

Therefore:

- topology comes only from the EMR PartList child lists;
- missing banks stay missing (zero-size sections), not synthesized;
- no fixed human bone names or parent map are created;
- renderer/importer code must keep mesh object count separate from animation-part count.

Use `re1c_bank_available()`, `re1c_get_bank_info()` and the new `re1c_stats.bank_*_object_count` fields rather than assuming `stats.object_count` is a skeleton size.
