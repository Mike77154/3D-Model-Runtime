# EM1111.EMD - referencia verificada

El codec de origen `recodec_c89` se compiló y se ejecutó sobre el archivo entregado. Su viewer RE1 EDD/EMR + texture pipeline produjo correctamente una vista animada de Bank B, clip 3, frame 10.

## Secciones

```text
file size 189132
EMR_A  off 0x00000000 size 22540
EDD_A  off 0x0000580C size 1040
EMR_B  off 0x00005C1C size 67260
EDD_B  off 0x000162D8 size 3088
TMD    off 0x00016EE8 size 28592
TIM    off 0x0001DE98 size 66592
DIR    off 0x0002E2B8 size 20
```

## Mesh

```text
15 TMD objects / rigid pieces
352 vertices total
900 normals total
648 primitive packets
648 packets are 0x34000609 textured triangles
0 invalid primitives in the tested model
```

Object counts (vertex / normal / primitive):

```text
00 20 / 102 / 34
01 66 / 100 / 128
02 73 / 73 / 142
03 17 / 90 / 30
04 15 / 78 / 26
05 16 / 78 / 26
06 17 / 17 / 30
07 15 / 15 / 26
08 16 / 16 / 26
09 14 / 72 / 24
10 22 / 22 / 52
11 17 / 88 / 30
12 14 / 72 / 24
13 13 / 60 / 20
14 17 / 17 / 30
```

## Skeleton / rigid hierarchy

Bank B PartList parents:

```text
00 -1
01  0
02  1
03  1
04  3
05  4
06  1
07  6
08  7
09  0
10  9
11 10
12  0
13 12
14 13
```

Esto es el árbol que se debe usar. El stack humano hardcodeado viejo no es autoridad.

## Animation banks

```text
Bank A: part_list=0x64 anim=0xB0 objects=15 frame_size=0x68 poses=215 clips=12
Bank B: part_list=0x64 anim=0xB0 objects=15 frame_size=0x68 poses=645 clips=31
```

Cada pose usa 104 bytes de stride. La parte útil observada es:

```text
12 bytes root payload
15 * 6 bytes angles
2 bytes tail/pad
```

EDD selecciona poses con `pose_index` y conserva `step_units` por frame.

Prueba puntual del viewer y de RE1codec:

```text
Bank B clip 3 frame 10
clip frames = 42
pose_index = 82
step_units = 1
root centi = 143, 1860, -263
```

## Texture

```text
embedded TIM
mode = 8bpp indexed
image = 256 x 256
CLUT = 256 x 2 at VRAM y=480
CBA 0x7800 -> 348 primitives
CBA 0x7840 -> 300 primitives
TSB 0x0080 -> 348 primitives
TSB 0x0081 -> 300 primitives
UV local range observed: U 0..127, V 0..254
```

La textura no se resuelve por un pathname. Está adentro del EMD y cada primitive decide palette/page mediante CBA+TSB.
