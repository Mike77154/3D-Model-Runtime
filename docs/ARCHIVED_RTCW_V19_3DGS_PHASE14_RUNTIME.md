> HISTÓRICO: documentación de la versión anterior; para Fase15 usar `RTCW_V19_3DGS_PHASE15_RUNTIME.md`.

# Runtime 75 — RTCW v19 + 3D GameStudio Fase14 (C89)

## Cambios

- RTCW: se sustituyó el directorio `vendor/rtcw_natsuki_v15/` por `vendor/rtcw_natsuki_v19/`. Permanece el proveedor 105 y sus llamadas originales de geometría MD3/MDC/MDS. El toolkit completo v19, incluido el auditor por criatura y funciones VFS/PK3, se incluye como fuente, pero **el proveedor 105 solo enlaza `rtcw_codec.c` y `rtcw_mds_pose.c`**; no se confunde tener la herramienta con exponer todo en el visor.
- GameStudio: nuevo proveedor 106 (`src/providers/provider_3dgs89.c`) con sondeo de firma para `MDL3`, `MDL4`, `MDL5` y `MDL7`. Se conservó el antiguo proveedor 84 de MDL7 como respaldo; el enrutador permite acudir a él si falla la apertura del nuevo.
- El núcleo Fase14 está incluido en `vendor/3dgs_phase14/`. Se compila desde `SOURCES_C89.txt`. El puente VTK/Python de Fase14 permanece como herramienta, no es dependencia del runtime C89.

## Funciones conectadas en el proveedor 106

- Mallas, triángulos y UV nativos; grupos separados; materiales/slots de skin; catálogo completo de skins; RGB(A) interno desde formatos de skin soportados, incluidos DDS DXT1/3/5 y ARGB sin DX10, con salida RGBA proporcionada por el llamador.
- Los formatos MDL3/4/5 usan fotogramas de vértices (morph), no huesos. La marca `GS3D_WEIGHT_NO_BONE` nunca se envía como influencia al renderer.
- MDL7 utiliza el deformer Fase14 si está disponible y, en caso de error, intenta frame bruto para mantener un resultado diagnóstico. Las poses esqueléticas siguen siendo experimentales, sin certificación de coincidencia visual con Acknex.
- Cada grupo expone su secuencia de frames por índice mediante `modelrt_visit_animation_frame` sin inferir FPS. No se inventan clips de animación a partir de nombres.
- Como el codec de origen es singleton/almacenamiento estático, el puente vuelve a abrir el archivo en cada operación: no es thread-safe ni reentrante. Las skins externas sin callback se catalogan pero no se decodifican automáticamente; el registro de materiales no es sinónimo de resolución de texturas externas.

## Pruebas efectuadas (Linux GCC)

1. Compilación completa `make -j6 libmodel_runtime89.a` completada.
2. RTCW V19: `make all` y `sh RUN_ALL_TESTS.sh` completados; suite final de auditoría V19 con 65 validaciones.
3. GameStudio Fase14: `sh scripts/check_phase14.sh` completado con fixtures generados.
4. Runtime 3DGS: aperturas y visitas MDL3/4/5/7, frames y texturas sintéticas, incluido DDS; 20 casos válidos verificados.
5. Regresiones `rtcw_re4_v52_runtime_regression` (MDS/MDC/MD3 + RE4 SEQ) e IQM interpolado completadas.

## Limitaciones

No se certificó MinGW32, el material de juegos reales ni igualdad exacta de skinning/pose con 3D GameStudio. RTCW shaders, PK3/VFS y auditoría v19 se conservan en el toolkit fuente pero no se enlazan automáticamente al visor.

## Reproducción de pruebas

```sh
make -j6 libmodel_runtime89.a
make gs3d-phase14-runtime-test
(cd vendor/rtcw_natsuki_v19 && make -j4 all && sh RUN_ALL_TESTS.sh)
python3 tests/generate_rtcw_re4_fixture.py
make rtcw-re4-v52-test
./tests/rtcw_re4_v52_runtime_regression tests/rtcw_re4_fixtures/synthetic.mds tests/rtcw_re4_fixtures/synthetic.mdc tests/rtcw_re4_fixtures/synthetic.md3 tests/rtcw_re4_fixtures/synthetic.seq
make native-iqm-interpolation-smoke
```

**Distribución:** fuentes, cabeceras, pruebas y documentación. Los modelos sintéticos se regeneran por script, no están dentro del ZIP de entrega.
