# RE1 EMD EDD/EMR animation probe patch

This patch tests RE1-style EMD animation banks using the five 32-bit offsets stored at the end of the file.

## Logical section split

For `EM1111(5).EMD`, the tail directory is interpreted as:

```text
EMR_A: 0x00000000 - 0x0000580B
EDD_A: 0x0000580C - 0x00005C1B
EMR_B: 0x00005C1C - 0x000162D7
EDD_B: 0x000162D8 - 0x00016EE7
TMD:   0x00016EE8 - 0x0001DE97
TIM:   0x0001DE98 - 0x0002E2B7
DIR:   0x0002E2B8 - 0x0002E2CB
```

## What changed

- `src/giffy_recodec.c`
  - Adds RE1 EMD five-offset tail directory detection.
  - Exposes six logical sections: EMR_A, EDD_A, EMR_B, EDD_B, TMD, TIM.
  - Allows the EMD loader to choose the mesh from section 4 when the six-section RE1 layout is detected.

- `tools/emd_textured_viewer.c`
  - Adds the same six-section RE1 EMD split.
  - Adds CLI flags:
    - `--anim`
    - `--bank A|B`
    - `--clip N`
    - `--frame N`
  - Applies a proof-of-concept EDD/EMR pose to the limb hierarchy and renders animated frames.

## Example commands

```sh
./build/grc sections /path/to/EM1111.EMD
./build/grc info /path/to/EM1111.EMD
./build/grc validate /path/to/EM1111.EMD

./build/emd_textured_viewer /path/to/EM1111.EMD frame0.bmp front --bank B --clip 0 --frame 0
./build/emd_textured_viewer /path/to/EM1111.EMD frame10.bmp front --bank B --clip 0 --frame 10
```

## Test result

The generated frames differ strongly across the selected clip/frame sequence, which confirms that the animation bytes are being read and applied to the rendered model.

## Limitations

This is intentionally a proof-of-concept patch, not a final GACKT animation exporter yet.
The EDD/EMR decoding currently applies a plausible PSX-style transform interpretation, but some frames can look contorted. The remaining production work is to tune the exact axis order, signed field interpretation, root handling, and export the decoded motion as proper GACKT clips/tracks.
