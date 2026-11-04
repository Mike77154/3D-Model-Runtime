# RE1codec v0.3-mvref

Codec/decoder dedicado a modelos de Resident Evil 1 de PS1 (`EMD`, `EMW` y `TMD`), construido sobre la ruta RE1 ya comprobada en `recodec_c89` y ampliado contra el corpus real suministrado.

## Reglas de runtime

- C89 estricto.
- Sin `malloc`, `realloc` ni liberación de heap.
- Sin tipos de punto flotante.
- El archivo se entrega al codec como un buffer propiedad del caller.
- Posiciones y rotaciones se exponen como enteros exactos y fixed-point Q16 cuando se solicita.
- La librería no modifica el buffer de entrada.

## Qué decodifica

```text
EMD trailing directory
  EMR_A -> pivotes, PartList/hierarchy, poses (opcional)
  EDD_A -> clips, pose index, unknown_word crudo (opcional)
  EMR_B -> pivotes, PartList/hierarchy, poses
  EDD_B -> clips, pose index, unknown_word crudo
  TMD   -> piezas, vertices, normals, primitive packets, UV/CBA/TSB
  TIM   -> CLUT + indexed pixels

EMW trailing directory
  EMR_B -> animación/jerarquía única
  EDD_B -> clips
  TMD   -> weapon mesh
  TIM   -> no embebido

standalone TMD
  TMD   -> mesh sin bank ni TIM
```

La animación es rígida por piezas. Cada pose trae root translation y 3 ángulos de 12 bits por parte animada. El árbol real sale de `EMR PartList`; no se usa un esqueleto humano hardcodeado. **TMD object_count y EMR object_count se mantienen separados**: no se inventan huesos para hacer coincidir ambas cantidades.


## Apertura de formatos y banks opcionales

```c
re1c_u32 kind;
re1c_view view;
int rc = re1c_open_model(bytes, size, &view, &kind);
```

`re1c_open_model()` detecta `EMD`, `EMW` y `TMD`. También existen `re1c_open_emd()`, `re1c_open_emw()` y `re1c_open_tmd()` para rutas explícitas.

`re1c_bank_available()` distingue un bank ausente de uno corrupto y `re1c_get_preferred_bank()` elige B si existe, A como fallback. Esto importa porque muchos EMD del juego no contienen bank A.

El corpus suministrado contiene tres disposiciones EMD: 34 archivos A+B de directorio de 20 bytes, 32 B-only con dos offsets iniciales en cero y un B-only con directorio compacto de 16 bytes. Véase `docs/EMD_VARIANTS.md`.

## Coordenadas

Se conserva la transformación usada por RE1MV:

```text
position_word:
  word > 32767 -> +(65535-word) centi-units
  otherwise    -> -word centi-units

TMD file X,Y,Z -> render/world Z,Y,X
EMR pivot Z,Y,X -> render/world X,Y,Z
pose root file X,Y,Z -> render/world Z,Y,X
```

`re1c_centi_to_q16()` produce Q16.16 únicamente con enteros.

## Animación / tiempo

EDD tiene registros de 4 bytes:

```text
u16 pose_index
u16 unknown_word
```

El source real de RE1MV (`edd.pas`) llama al segundo WORD `Unknow` y no lo usa para temporizar. RE1MV avanza exactamente una entrada EDD por tick de `Timer4`, cuyo `Interval` en `main.dfm` es 40 ms: **25 FPS fijos**. Por eso v0.3 añade `re1c_get_mv_frame_duration_ms()` y `re1c_get_clip_duration_ms()`. El antiguo campo `step_units` se conserva sólo como alias de compatibilidad de v0.2; no debe interpretarse como duración.

Los ángulos siguen la conversión exacta del MV: `angle12 / 0xFFF`, no `/ 4096`.

## Textura

En `EM1111.EMD` la textura **no tiene nombre de archivo**: es un `TIM` embebido. La etiqueta API `embedded:TIM` es sólo un locator descriptivo. En `EMW` y `TMD` standalone el TIM puede estar ausente; `re1c_get_tim_info()` devuelve `RE1C_ERR_NOT_PRESENT`, permitiendo que el runtime conecte un provider/textura externa sin fingir que existe dentro del modelo.

Cada primitive conserva:

```text
UV bytes
CBA / CLUT address
TSB / texture page
raw variable-size primitive packet (28 bytes FT3, 36 bytes FT4)
vertex + normal indices
```

Para 8bpp, TSB page X avanza 128 pixels por page. CBA determina la fila/columna de CLUT. Esto evita el error típico de tratar todo el modelo como una sola UV sobre una paleta global.

## Verificación byte a byte

`re1c_canonical_build()` genera un stream binario determinista con:

- jerarquía, pivotes, clips, frame refs y todas las poses de ambas banks;
- raw + posición interpretada de vértices;
- normals raw;
- packet completo de cada primitive;
- metadata TIM + CLUT + pixel data.

`re1c_compare_views()` construye ambos streams y localiza el primer byte distinto. `re1c_compare_canonical()` hace la misma comparación sobre streams ya producidos, para que un importador externo pueda implementar su adaptador sin depender del parser EMD. Así cualquier provider se prueba antes de llegar al renderer.

## Build

```sh
make
make check EMD=/ruta/EM1111.EMD
./build/re1codec_inspect /ruta/EM1106.EMD
./build/re1codec_mv_audit /ruta/EM1106.EMD
./tests/audit_mv_reference_corpus.sh /ruta/ENEMY
./tests/audit_mv_reference_all.sh /ruta/ENEMY /ruta/PLAYERS
./build/test_ft4_packet
./tests/audit_constraints.sh
```

En MinGW/MSYS2 también se incluye `build_mingw.bat`.

## Corpus comprobado

```text
67/67 EMD enemigos
32/32 EMW weapons/player assets
 7/ 7 TMD standalone
------------
106/106 model files
0 primitive packets unsupported
```

El Golden Master de `EM1111.EMD` permanece byte-idéntico a v0.1 (`283734` bytes, FNV-1a32 `C2104AE0`).

## Referencia RE1MV real (v0.3)

`docs/MV_REFERENCE_AUDIT.md` documenta la comparación contra el source real entregado. El harness independiente `tools/re1codec_mv_audit.c` replica las lecturas de `Load_Emd`, la ruta EMW de `main.pas`, `Load_Emr`, `Load_Edd` y `Load_Tmd` y confronta sus resultados con la API pública del codec. El corpus completo vuelve a quedar en **106/106 PASS**: 67 EMD, 32 EMW y 7 TMD; 5,282,993 comprobaciones estructurales/campo, 133 banks, 2,098 clips, 39,539 frame refs, 37,185 poses, 560,075 registros parte-pose y 43,243 primitives.

El codec **no copia bugs del viewer**: el source de RE1MV desincroniza `EMD_Type=$10001` al aplicar `Skip_Byte=true` pese a `FrameSize=48`, y su `Build_Model` contiene jerarquías OpenGL hardcodeadas. v0.3 usa `FrameSize` y `EMR PartList`, que son los datos del archivo.
