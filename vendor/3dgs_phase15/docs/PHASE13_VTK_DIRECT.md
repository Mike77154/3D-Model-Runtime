# Fase 13 — captura VTK directa desde MDL3/4/5/7

## Objetivo

No convertir `.mdl` a OBJ/FBX/GLB para la previsualización. La biblioteca C89 decodifica la geometría y la textura; un puente de solo lectura (`src/bridge/gs3d_vtk_bridge.c`) expone sus datos numéricos sin crear geometría intermedia. `tools/vtk_capture.py` utiliza `ctypes` + VTK para construir `vtkPolyData` directamente en memoria y tomar una captura de la escena con `vtkWindowToImageFilter` y `vtkPNGWriter`.

El motor C89 conserva todas las restricciones habituales: sin heap, sin `malloc/realloc/calloc/free`, sin `float`, `double` o `long long` en los módulos nuevos. El visor Python/VTK sí utiliza coma flotante y objetos VTK, fuera del runtime C89, para dibujar la escena.

## API del puente

- `gs3d_vtk_group`: conteos de geometría, skins y frames.
- `gs3d_vtk_vertex`: posiciones Q16.16 + hueso, sin copiar el array completo.
- `gs3d_vtk_uv`: UV Q16.16.
- `gs3d_vtk_triangle`: índices locales de triángulos, UV y material, verificados.
- `gs3d_vtk_skin`: decodifica textura al buffer RGBA estático; **el siguiente decode sobrescribe ese mismo buffer**.
- `gs3d_vtk_weights`: pesos normalizados Q16.16 por vértice (máximo configurable).
- `gs3d_vtk_bone`: jerarquía y bind translations.
- `gs3d_vtk_frame_name`: nombres sin lectura fuera de rango.
- `gs3d_vtk_audit`: validación de índices, triángulos degenerados y estadísticas ponderadas.

## Compilar, probar, capturar

```sh
sh scripts/check_phase13.sh
GS3D_RUN_SANITIZER=1 CC=clang sh scripts/check_phase13.sh
sh scripts/build_vtk_bridge.sh
python3 tools/vtk_capture.py /ruta/Tusker.mdl /salida/Tusker_VTK.png
python3 tools/vtk_capture.py /ruta/Tusker.mdl /salida/Tusker_frame30_DIAGNOSTICO.png --frame 30 --animate
```

Las dependencias de Python/VTK son **solo para visualización**, no para importación en el Game Runtime. La compilación del puente compartido es para Linux, probada con GCC en el entorno actual. No se ha validado la compilación MinGW32 real.

## Resultados con corpus entregado

| Archivo | Vértices | Triángulos | Frames | Texturas decodificadas | Pesos explícitos |
|---|---:|---:|---:|---:|---:|
| Tusker | 1921 | 3732 | 139 | 4 | 5953 |
| TuskerSpear | 45 | 86 | 139 | 4 | 0 |
| NinjaGirlShinobi1 | 410 | 816 | 120 | 1 | 0 |
| Goddess4arms | 2625 | 2580 | 37 | 1 | 0 |
| LadyJ | 2377 | 2016 | 1 | 1 | 0 |
| ShinobiJ | 1759 | 1860 | 1 | 1 | 0 |

Los seis modelos MDL7 se abren y renderizan en reposo usando el códec C89 + VTK directo. La auditoría de índices encontró **cero triángulos con vértices repetidos** en las seis muestras; eso no descarta otros tipos de geometría degenerada o errores de topología. Tusker contiene 1,921 vértices con pesos explícitos.

## Limitaciones importantes

Las poses esqueléticas auténticas de MDL7 NO están certificadas. En particular, el fotograma 30 de Tusker revela estiramientos artificiales de brazos al aplicar el skinning existente. Las capturas animadas se etiquetan *POSE EXPERIMENTAL - NO VALIDADA* y no deben considerarse reproducción idéntica a Acknex/MED. La ruta del visor no corrige todavía esa convención de pivotes, matrices y deformadores. El VTK viewer mejora la observabilidad y el aislamiento de esos defectos.

Las capturas con `--exposure` (por defecto 1.18) aplican una corrección no destructiva de brillo en el visor; las texturas originales no se modifican. Los PNG son capturas con iluminación de VTK, no representaciones bit-a-bit del material de Acknex.

El código C89 sigue independiente del Model Runtime principal. Las capturas distribuidas NO incluyen los modelos MDL originales ni mallas convertidas.
