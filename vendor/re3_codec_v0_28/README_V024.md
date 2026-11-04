# RE3 Codec v0.24 — wider PC asset coverage (C89)

## New support

Existing enemy EMD/TIM decoding is unchanged. A separate read-only `re3_packed` module recognizes **five-section PLD containers** and **nine-section PLW containers** from RE3 PC. It validates the directory and reads embedded MD2 geometry and TIM texture resources. For PLD, it also decodes EDD/EMR animation data and applies DAT binding maps when validated. The library uses static structures, caller-owned scratch buffers and no heap allocation.

The additional library API is in `include/re3_packed.h`, with implementation `src/re3_packed.c`.

### Build

On Linux/MSYS2: `make` (ISO C89: `-std=c89 -pedantic -Wall -Wextra -Werror`). On Windows with MinGW32: run `build_mingw.bat`. No additional third-party library is needed for the new packed parser.

### Usage

```text
re3packed_info PL00.PLD
re3packed_preview PL00.PLD pl00.bmp front texture main
re3packed_motion_audit PL00.PLD

re3packed_info PL00W00.PLW
re3packed_preview PL00W00.PLW weapon.bmp quarter solid main
```

`re3packed_preview` can use an optional final `rebase` argument to explore a **provisional** TIM/VRAM offset for PLW, but **this is not a proven Capcom atlas mapping**. PLW animation chunks are not treated as standalone actor skeletons. Character + weapon runtime attachment remains an outstanding task.

Optional cross-platform batch regression (Python 3 **only for tests**, not runtime):

```text
python scripts/scan_re3_corpus.py --enemy /path/to/EMD_ROOT --packed /path/to/PLD_FILES --output results.csv
```

### What the supplied corpus actually demonstrated

| Asset family | Files | Read result | Extra checks |
|---|---:|---|---|
| EMD | 100 | 100 success | animation, morph, texture, mesh audits |
| TIM | 100 | 100 success | TIM decoder tests; EMD UV sampling |
| PLD | 27 | 25 success; 2 intentionally rejected | 25 PLD DAT bindings valid; 25 sample-animation audits passed |
| PLW | 84 | 84 geometry / embedded TIM recognized | no standalone animation or proven VRAM placement |

The exceptions were `PL0D.PLD` (eight-byte stub/invalid header) and `PL06CH.PLD` (valid container header but invalid MD2 data offsets). Neither is silently accepted as a complete model.

**Limits:** Passing these checks does **not** establish pixel-identical rendering, full morph-event timing, precise weapon texture atlas placement, correct damage-bank retargeting, or compatibility with an arbitrary resource outside this corpus. Some visual previews, especially `PL0F.PLD`, still require investigation. Do not distribute copyrighted game models along with the source code.
