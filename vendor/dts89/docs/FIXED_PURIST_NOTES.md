# dts89 fixed-purist notes

This revision removes C real-number types from the public API and implementation.

## What changed

- Removed direct C real-number API entry points.
- Replaced 1.0 defaults with raw IEEE-754 binary32 constants.
- Replaced binary32 bit-pattern conversions with integer-only routines.
- Replaced Q16.16 multiply/divide helpers with integer-only implementations.
- Replaced old billboard-angle calculation with a Q16.16 path.

## Preserved behavior

Torque DTS/DSQ files still store many values as 32-bit IEEE-754 binary32 payloads. dts89 keeps those payloads as `dts89_u32` so assets can roundtrip without requiring host C real-number arithmetic.

## Important API note

Use `DTS89_F32_BITS_ONE` for a raw binary32 one payload and `dts89_fx_to_f32bits()` / `dts89_f32bits_to_fx()` when crossing between raw engine payloads and Q16.16 values.
