# RE1codec v0.3-mvref — audit contra RE1MV real

## Referencia usada

Archivo entregado: `RE1MVRealSource.zip`

SHA-256:

```text
744f72117177fb59903f353aba5f6e0b6ab9f3ff9c47426427bcd71e2e338981  RE1MVRealSource.zip
0e1045893b5e5e464c25bbacef0bf350c54fb87fc9910cf69ed55812d8a539c7  emr.pas
9a964609dfa73d57df73654b91074f98dc6ca7a4b5d3c3dfedc5626abfd31ee3  edd.pas
007f92f33a1e56758faf2ae40db41c29fcba2bd436a4a77111a989fb7f63c980  tmd.pas
6444def1b6c304b19f10f37516b2a615c31a5304d8bf2eee70d05b456b2bf9f0  emr_type.pas
e08c71f2c851b54364d0c0e4080e45185d1822454d4ef18fe032c0f3a7c468da  main.pas
```

El source de terceros no se redistribuye dentro del codec. El harness incluido implementa de forma independiente las lecturas necesarias para poder repetir la comparación.

## Qué confirma el source

### Contenedor EMD

`main.pas::Load_Emd` lee cinco DWORD al final del EMD y separa:

```text
[0, off0)       EMR bank A
[off0, off1)    EDD bank A
[off1, off2)    EMR bank B
[off2, off3)    EDD bank B
[off3, off4)    TMD
[off4, EOF-20)  TIM
```

Los banks A pueden estar vacíos. `em1032.emd` es el caso raro donde la lectura de 20 bytes toma un DWORD previo al directorio real; el viewer sobrevive porque `off1=0`. v0.3 recorta TIM a su tamaño declarado, que coincide exactamente con el stream que RE1MV entrega a `Tim.dll`; así elimina los 4 bytes de padding que v0.2 incluía en ese caso compacto.

### Contenedor EMW

La rutina `ToolButton47Click()` de `main.pas` confirma el directorio de 8 bytes al final del EMW:

```text
[0, off0)       EMR
[off0, off1)    EDD
[off1, EOF-8)   payload TMD
[EOF-8, EOF)    dos offsets DWORD
```

El viewer usa esa ruta para importar animación; v0.3 además valida el payload restante con el mismo parser TMD confirmado por `tmd.pas`.

### EMR: pivotes, jerarquía y poses

`emr.pas` define el header exacto:

```text
u16 PartListOffset
u16 AnimationOffset
u16 ObjectCount
u16 FrameSize
```

Cada pose contiene 12 bytes de root/metadata seguidos por `ObjectCount` triples de ángulos de 16 bits. RE1MV enmascara cada ángulo con `0x0FFF`.

Las posiciones usan la función `Calculate_Pos()` del viewer. En centi-units equivale a:

```text
word > 32767 : +(65535 - word)
otherwise    : -word
```

La jerarquía de datos está en `PartList`. `EMR_Part()` y `Load_Ans()` leen esa lista directamente. v0.3 añade `re1c_get_bind_global_centi()` para reconstruir la posición global de bind acumulando padres sin usar floats.

### Rotaciones

RE1MV convierte a grados con divisor `0xFFF` (4095), no 4096:

```text
angle_degrees = (angle12 / 4095) * 360
```

Y al renderizar remapea los ejes:

```text
file AngleX -> rotación alrededor de -Z
file AngleY -> rotación alrededor de -Y
file AngleZ -> rotación alrededor de -X
```

v0.3 corrige `re1c_angle12_to_turn_q16()` para reproducir esa conversión exacta sin punto flotante.

### EDD: clips y frame refs

`edd.pas` obtiene el número de clips desde el primer `FrameStart`:

```text
Animation_Count = first_stream_offset / 4
```

Cada header de clip es:

```text
u16 FrameCount
u16 FrameStart
```

Y cada entrada del stream es:

```text
u16 FrameIndex
u16 Unknow
```

El segundo WORD **no se usa para timing en RE1MV**. v0.2 lo llamaba `step_units`; v0.3 lo renombra semánticamente a `unknown_word` y mantiene `step_units` sólo como alias de compatibilidad.

### Timing real del viewer

`main.dfm` define:

```text
Timer4.Interval = 40
```

`Timer4Timer()` incrementa exactamente una posición del scrollbar por tick. `Render_Scene()` obtiene el pose index desde la entrada EDD correspondiente. Resultado: el viewer reproduce a **25 FPS fijos**, o 40 ms por frame ref.

APIs nuevas:

```c
re1c_get_mv_frame_duration_ms();
re1c_get_clip_duration_ms();
```

### TMD: meshes y primitive packets

`Load_Tmd()` confirma:

- object descriptor: 28 bytes;
- vertex: 8 bytes;
- normal: 8 bytes;
- FT3 `$34000609`: packet de 28 bytes;
- FT4 `$3C00080C`: packet de 36 bytes.

El bug de v0.2 era calcular `primitive_offset + primitive_index * 28`. Eso sólo funciona mientras todos los packets sean FT3. v0.3 camina cada packet usando el byte `ILEN` del propio packet:

```text
packet_size = 4 + ILEN * 4
```

Así un quad de 36 bytes ya no desplaza el packet siguiente. `tests/test_ft4_packet.c` prueba específicamente `FT4(36) + FT3(28)` en el mismo objeto.

## Cómo se hizo la comparación

`tools/re1codec_mv_audit.c` es un parser de referencia separado del codec. No llama funciones privadas de `re1codec.c` para calcular los valores esperados. Lee los bytes según las operaciones del source de RE1MV y después compara contra la API pública:

```text
re1c_open_model
re1c_get_bank_info
re1c_get_pivot
re1c_get_bind_global_centi
re1c_get_child_count / re1c_get_child
re1c_get_clip / re1c_get_frame_ref
re1c_get_pose_root / re1c_get_pose_angle
re1c_get_tmd_object
re1c_get_vertex_raw / re1c_get_normal_raw
re1c_get_primitive
re1c_get_mv_frame_duration_ms
re1c_get_clip_duration_ms
```

También hace `memcmp()` de las secciones crudas EMR_A, EDD_A, EMR_B, EDD_B, TMD y TIM contra los slices que produciría `Load_Emd`.

## Resultado sobre el corpus completo (106 modelos)

```text
files                    106 PASS / 0 FAIL
  EMD enemies             67
  EMW player/weapons      32
  standalone TMD           7
banks                    133
parts                    1,955
hierarchy child refs     1,822
clips                    2,098
EDD frame refs           39,539
EMR poses                37,185
part-pose angle records  560,075
TMD objects              1,027
vertices                 24,995
normals                  55,103
primitive packets        43,243
field/structure checks   5,282,993
raw-byte comparisons     11,084,648
```

`raw-byte comparisons` incluye el `memcmp` de secciones completas y una segunda comprobación de cada primitive packet, por lo que algunos bytes se verifican más de una vez intencionalmente.

`EM1106.EMD` individualmente:

```text
checks=116144
banks=2
parts=31
clips=29
frame_refs=881
poses=818
part-pose angle records=12983
objects=16
vertices=457
normals=893
primitives=792
```

## Bugs/quirks del MV que NO se copiaron

### 1. `Skip_Byte` se equivoca con `$10001`

`Check_Id()` activa `Skip_Byte=true` por defecto y sólo lo desactiva para cinco IDs. `$10001` no está en esa lista.

Sin embargo `EM1013.EMD` y `EM1113.EMD` tienen:

```text
EMD_Type   = 0x00010001
ObjectCount = 6
FrameSize   = 48
12 + 6*6    = 48
```

No existe el padding de 2 bytes. El loop secuencial de `Load_Emr()` del MV intenta avanzar 50 bytes por pose aunque el header declara 48, por lo que se desincroniza. El harness lo reporta como `mv_stride_quirks=1` para esos dos archivos, pero el codec **no falla**: usa `FrameSize`, que es la estructura real del archivo.

### 2. `Build_Model()` es una jerarquía de renderer hardcodeada

`emr_type.pas::Build_Model()` contiene árboles OpenGL escritos a mano por `EMD_Type`. No es el parser de la jerarquía: el parser real está en `PartList`. v0.3 conserva como verdad estructural el `PartList` del EMR y no incorpora ese switch gigante al codec.

Esto además permite que criaturas no humanoides funcionen sin fingir un esqueleto humano.

### 3. Normales FT4

El branch FT4 del source avanza por los WORD de normal pero sólo guarda los índices de vértice. v0.3 conserva también los cuatro normal indices del packet; no elimina información sólo para reproducir una limitación del viewer.

## Repetir el audit

```sh
make
./build/re1codec_mv_audit /ruta/ENEMY/EM1106.EMD
./tests/audit_mv_reference_corpus.sh /ruta/ENEMY
./tests/audit_mv_reference_all.sh /ruta/ENEMY /ruta/PLAYERS
./build/test_ft4_packet
./tests/audit_constraints.sh
```

Los logs completos están en `docs/MV_REFERENCE_CORPUS_RUN.txt` (67 EMD) y `docs/MV_REFERENCE_ALL_106_RUN.txt` (106 modelos).
