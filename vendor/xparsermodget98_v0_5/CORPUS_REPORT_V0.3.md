# v0.3 external corpus report

This report records the compatibility run used to harden v0.3.  The external
asset corpus is not redistributed with the parser.

## Results

| Encoding | Pass | Reject | Total |
|---|---:|---:|---:|
| txt | 104 | 0 | 104 |
| bin | 3 | 0 | 3 |
| bzip | 89 | 3 | 92 |
| **Total** | **196** | **3** | **199** |

Corpus groups: `generic` 37/37, `ww2` 43/43, `xandy.zip` 36/36,
`oldchars` 60/62 and `Zombie Apocalypse` 20/21.

## Inputs rejected as malformed compression

The three remaining rejects are not grammar mismatches.  Their MSZIP framing
is readable, but independent Python zlib raw-DEFLATE decoding fails at the same
blocks with `invalid stored block lengths`:

- `oldchars/.../hostage.x`: block 272, after 8,912,896 decoded payload bytes.
- `oldchars/.../scout1.X`: block 375, after 12,288,000 decoded payload bytes.
- `Zombie Apocalypse/.../slowzombie3.X`: block 6, after 196,608 decoded payload bytes.

The decoder intentionally returns `XP_EFORMAT` instead of relaxing DEFLATE
integrity checks or inventing missing output bytes.

## Bugs found by the corpus and fixed in v0.3

- square brackets in text template declarations;
- semicolon-separated text face indices;
- real `.x` MSZIP block framing;
- packed binary integer/real lists spanning semantic reads;
- WORD binary string terminators;
- a legacy TextureFilename containing a literal horizontal TAB.

## Build/safety validation

GCC and Clang strict C89 builds pass for all eight feature-toggle combinations.
No allocator calls, C `float`/`double` declarations, or `long long` are present
in `include/`, `src/`, or `examples/`.  UBSan was run over all 199 files and
reported no undefined-behavior finding.  ASan+UBSan was run over packaged
fixtures plus a size-stress corpus sample and the three malformed inputs; no
sanitzer error was reported.
