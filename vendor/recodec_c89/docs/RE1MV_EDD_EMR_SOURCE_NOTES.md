# RE1MV EDD/EMR source notes applied to recodec

This pass was validated against `RE1MV` Delphi source (`edd.pas`, `emr.pas`, `main.pas`, `emr_type.pas`).

## EMD split

RE1 EMD uses a trailing 5-offset directory. For `EM1111(5).EMD`:

```text
EMR_A: 0x00000000..0x0000580B
EDD_A: 0x0000580C..0x00005C1B
EMR_B: 0x00005C1C..0x000162D7
EDD_B: 0x000162D8..0x00016EE7
TMD:   0x00016EE8..0x0001DE97
TIM:   0x0001DE98..0x0002E2B7
DIR:   0x0002E2B8..0x0002E2CB
```

## EMR header, per RE1MV `emr.pas`

```c
u16 part_list_offset;  /* +0 */
u16 animation_offset; /* +2 */
u16 object_count;     /* +4 */
u16 frame_size;       /* +6 */
```

The first experimental recodec patch used `+0` as frame stride; this was wrong.
For EM1111 both banks have:

```text
part_list_offset = 0x0064
animation_offset = 0x00B0
object_count     = 15
frame_size       = 0x0068
```

Frame stride is therefore `0x68`, matching RE1MV's read loop:

```text
12 bytes root position payload
15 * 6 bytes object angles
2 bytes pad/skip
```

## EDD layout, per RE1MV `edd.pas`

```c
struct EDD_Info {
    u16 frame_count;
    u16 frame_start_byte_offset;
};

struct EDD_Track {
    u16 frame_index;  /* pose index into EMR animation frames */
    u16 unknown;      /* timing/step/control; preserved as `frame_step` */
};
```

`Animation_Count = first_frame_start / 4` where `first_frame_start` is the word at `EDD+2`.
For a clip:

```c
frame_rec  = info.frame_start_byte_offset + (frame % info.frame_count) * 4;
pose_index = u16(EDD + frame_rec + 0);
frame_step = u16(EDD + frame_rec + 2);
pose       = EMR + animation_offset + pose_index * frame_size;
```

## Transform notes, per RE1MV `main.pas`

`Calculate_Pos()` is mirrored as `re1mv_calc_pos()`:

```text
if word > 32767: (65535 - word) / 100
else:            -word / 100
```

Bind triplets in EMR are read in file order `Z,Y,X` by RE1MV `Load_Ans()`.
Root motion is read as `X,Y,Z` but rendered with `glTranslatef(Z,Y,X)`.

Angles are 12-bit values. RE1MV converts them to degrees, then renders:

```text
glRotatef(AngleX, 0, 0, -1)
glRotatef(AngleY, 0, -1, 0)
glRotatef(AngleZ, -1, 0, 0)
```

The software viewer remaps this to the existing matrix helper as:

```text
X = -AngleZ
Y = -AngleY
Z = -AngleX
```

## Current status

The diagnostic textured viewer now renders animated frames using the verified RE1MV EDD/EMR semantics. This is not yet a final GACKT `ganim` exporter; it is a working proof and a more accurate source-compatible decode path.
