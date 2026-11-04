# API de clips — Fase 08

### Constructor de un clip

```c
GS3D_Clip walk;
GS3D_ClipSample sample;
int r;
r = gs3d_clip_from_names(&walk, 7U, 0U, "start", "end",
                         100UL, GS3D_CLIP_LOOP, GS3D_CLIP_TRACK_AUTO);
if (r == GS3D_OK) r = gs3d_clip_apply(&walk, 50UL, &sample);
```

`generation` es A4…A8, `group` identifica la malla a animar, `ticks_per_frame`
es una unidad explícita del llamador. Un ejemplo con 100 ticks por frame y
tick=50 interpola al 50 %, independientemente de la unidad de tiempo real.

### Modos

- `GS3D_CLIP_CLAMP`: al terminar la última transición, mantiene el último fotograma.
- `GS3D_CLIP_LOOP`: incluye la transición del último fotograma de vuelta al primero.
- `GS3D_CLIP_PINGPONG`: al llegar al final, invierte el sentido sin duplicar fotogramas de extremo.

La estructura `GS3D_ClipSample` contiene `frame_a`, `frame_b`, `alpha_q16`,
`direction` y `at_end`. Para el último frame en modo clamp, ambos índices son
iguales y `alpha_q16 = 0`.

### Controlador

```c
GS3D_ClipPlayer p;
gs3d_player_init(&p, &walk);
gs3d_player_advance(&p, 25UL);
gs3d_player_apply(&p, &sample);
p.playing = 0U; /* pausa */
gs3d_player_seek(&p, 100UL);
```

El contador de ticks no usa `long long`; si se intenta sumar un delta que
sobrepase `ULONG_MAX`, devuelve `GS3D_ERR_RANGE` y no modifica el estado.

### Secuencia OBJ

```sh
./gs3d_clip tests/fixtures/mdl7_skeletal_sequence.mdl export /tmp/pose 7 0 0 2 100 25 5 0 0
```

Se generan cinco poses: ticks 0,25,50,75,100. Se exportan `.obj` y `.mtl`
y TGA solamente si existe textura raster. El último argumento opcional `all`
aplica la misma secuencia a todos los grupos animados compatibles.

### Aclaraciones

Los archivos MDL7 usados para derivar estructuras describen nombres de frame,
conteos de vértices y matrices, pero no un FPS global único. Los nombres no
garantizan inicio/fin de clips. Por tanto, el runtime no adivina timing ni cortes.

La composición de matrices y pesos de huesos continúa siendo experimental
frente a casos reales de deformadores de varias influencias; solo los tests
sintéticos de esta fase verifican el resultado de la reproducción esquelética.
