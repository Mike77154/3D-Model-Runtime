# RE1MV Texture Pipeline v07

This patch attacks the UV/texture mismatch seen on RE1 EMD torso/arms.

## Main fix

The previous textured viewer decoded the embedded TIM into one global RGBA image using palette 0, then sampled `u,v` directly. That is not enough for RE1 PSX TMD primitives.

v07 now reads per primitive metadata from the RE1 TMD packet:

- `CBA` / CLUT per primitive
- `TSB` / texture page per primitive
- `u,v` values per vertex
- RE1MV-style FT3/GT3/FT4/GT4 packet families

Then it samples the TIM indexed pixels directly:

```text
primitive CBA + primitive TSB + local UV + raw TIM pixels + raw TIM CLUT = final color
```

This fixes the common case where head/legs look correct but torso/arms sample the wrong half/page/palette.

## RE1MV clue

RE1MV's Pascal code effectively does this for textured primitives:

```pascal
Read(U0,V0)
Read(Temp_Word)  { CBA / palette row }
if Temp_Word > 30720 then Rp := 0.5 else Rp := 0;
Read(U1,V1)
Read(TSB)
Read(U2,V2)
```

In PSX terms, this means the primitive is not just `UV`; it also carries the CLUT and texture-page state. On EM1111, the relevant words are commonly:

```text
CBA: 0x7800 / 0x7840  -> CLUT row 480 / 481
TSB: 0x0080 / 0x0081  -> 8bpp page 0 / page 1
```

For 8bpp texture pages, one page-X step is 64 VRAM words = 128 decoded pixels. That is why RE1MV's old `Rp = 0.5` on a 256-wide composite texture maps to the same half-page behavior.

## CLI

```bash
build/emd_textured_viewer EM1111.EMD out_front.bmp front
build/emd_textured_viewer EM1111.EMD out_side.bmp side
build/emd_textured_viewer EM1111.EMD out_iso.bmp iso
```

`--frame N` and `--pal N` are accepted for compatibility with the v06 CLI, but v07's RE1 raw sampling uses per-primitive CLUT/TPAGE where available.

## Still pending

This viewer still uses the existing geometry/bind-pose path from the codec. For exact 1:1 RE1MV posing, the next target remains the EMR animation-frame transform path rather than texture mapping.
