# Modelo Runtime 75: RTCW v15 + RE4 Classic v5.2

**Base:** `Modelo_Runtime_Nativo75_Quake2_MD2_PCX_CORPUS65_C89_SOURCE_ONLY.zip`.
**Build:** C89 GCC (`make -j6`), no runtime heap/malloc/free, no floating point, no long long.
**Proveedores:** RTCW 105 (nuevo); RE4 104 (actualizado de v4.1 a v5.2). Los IDs anteriores se conservaron.

## Integración RTCW

- `vendor/rtcw_natsuki_v15/` conserva el código fuente y herramientas de RTCW v15 (incluidos shader/material, PK3, VFS, skins, rigging y preview).
- `src/providers/provider_rtcw89.c` incorpora lectura nativa de `.mds` (MDSW v4), `.mdc` (IDPC v2) y `.md3` (IDP3 v15).
- En el runtime, MDS está preferido, MDC v2 utiliza el proveedor 105 cuando la cabecera corresponde, MD3 conserva primero el proveedor nativo anterior 56; RTCW sirve como fallback. Un error de apertura permite probar otros lectores.
- Geometría, UV Q16, materiales como referencias a nombres de shader, huesos MDS catalogados y acceso a fotogramas exactos mediante `modelrt_visit_animation_frame(h,0,frame,...)`.
- Las coordenadas Q6 y UV Q12 se convierten a Q16 con comprobación de rango. No se interpolan fotogramas ni se inventa frecuencia FPS.
- El proveedor usa el decodificador original con **estado estático global**; reabre y decodifica el archivo en cada visita. Seguro para alternar handles de forma serial, **NO thread-safe**. No se prometen poses concurrentes desde varios hilos.
- No implementa el renderizado de shaders RTCW en el visitor (el toolkit stand-alone sí mantiene animMap/alpha/PK3/VFS), ni las matrices completas de anclajes/tag, ni la lectura automática de skins `.skin` por el visor. Las normales se dejan ausentes cuando el codec no proporciona una por vértice.

## Integración RE4 Classic v5.2

- Código fuente del proveedor sustituido por `vendor/re4_classic_v5_2/`; el puente PMD/BIN/TPL/FCV existente permanece y continúa animando por vinculación explícita PMD+FCV.
- El runtime reconoce `.seq` como catálogo de eventos bruto del códec RE4 v5.2. Sus claves se exponen mediante `modelrt_re4_seq_key_count` y `modelrt_re4_seq_key_at`.
- `modelrt_re4_seq_seek` permite reconstruir estados visuales deterministas desde reglas **expresamente suministradas por quien llama** (mostrar/ocultar pieza, pose, asignar/liberar padre). Acepta tiempo Q6, periodo verificado y límites de bucles. El código 0 jamás se convierte en acción.
- No se ha deducido el significado nativo de eventos SEQ, ni el emparejamiento SEQ/FCV, ni identidades de Plaga A/B/C. Una regla hipotética no representa el juego original. Los estados visuales **no modifican automáticamente la geometría ni fijan matrices de anclaje**.
- En la biblioteca base no se han unido aún automáticamente los archivos de archivo `.dat`/`afs` con el modelo visible: son utilidades del códec V5.2, incluidas como fuente para consulta posterior.

## Reproducción de pruebas

```
make -j6
make rtcw-re4-v52-test
python3 tests/generate_rtcw_re4_fixture.py
# MDS/MDC/MD3 y SEQ sintéticos sin assets comerciales
./tests/rtcw_re4_v52_runtime_regression tests/rtcw_re4_fixtures/synthetic.mds tests/rtcw_re4_fixtures/synthetic.mdc tests/rtcw_re4_fixtures/synthetic.md3 tests/rtcw_re4_fixtures/synthetic.seq
make re4classic-synthetic
make re4classic-animated-synthetic
make gunsurvivor-synthetic
make hod2-synthetic
make hod2-phase12-tables
make native-iqm-interpolation-smoke
make -C vendor/rtcw_natsuki_v15 CC=gcc -j4
python3 vendor/rtcw_natsuki_v15/tests/test_codec_v15.py ./vendor/rtcw_natsuki_v15/rtcw_rig ./vendor/rtcw_natsuki_v15/rtcw_matprobe
make -C vendor/re4_classic_v5_2 -j4 check
```

## Resultados de esta integración

- Runtime GCC C89 biblioteca: PASS.
- Test del puente: MDS dos frames, MDC dos frames, MD3 dos frames, RE4 `.seq` y estado visual: PASS (fixtures sintéticos).
- Regresiones: `re4classic-synthetic`, `re4classic-animated-synthetic`, `gunsurvivor-synthetic`, `hod2-synthetic`, `hod2-phase12-tables`, `native-iqm-interpolation-smoke`: PASS.
- Herramientas RTCW v15 y test v15: PASS.
- Suite nativa RE4 v5.2: PASS.
- NO se ha realizado prueba visual exhaustiva con modelos reales RTCW / RE4 ni prueba MinGW32.

**Contenido del paquete:** código fuente, documentación, scripts de pruebas y datos de tabla HOD2 ya presentes en el runtime. Ningún modelo, textura ni ejecutable comercial.
