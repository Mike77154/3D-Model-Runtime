# 3D GameStudio A4–A8 — Phase 15 (source only)

Extiende la Fase 14 conservando los vendors C89.

## Elegir espacio de matrices MDL7

```c
if (gs3d_open("Tusker.mdl") == GS3D_OK) {
    gs3d_set_pose_mode(GS3D_POSE_FILE_SPACE);
    gs3d_apply_deformer(0U, 30U);
    /* gs3d_model.v[] contiene la pose; VTK lee por su bridge. */
}
```

`GS3D_POSE_HIERARCHY` conserva el comportamiento anterior y es el modo por defecto de biblioteca por compatibilidad. `GS3D_POSE_FILE_SPACE` aplica las matrices del fichero con compensaciones de pivote ya incluidas. No interpreta por la fuerza las matrices como transformaciones locales.

```sh
sh scripts/check_phase15.sh
python3 tools/vtk_capture.py /ruta/Tusker.mdl captura.png --animate --frame 30 --pose-mode file
python3 tools/vtk_capture.py /ruta/Tusker.mdl diagnostico.png --animate --frame 30 --pose-mode legacy
```

Sin conversiones intermedias, solo VTK para render PNG. Corpus binario auténtico excluido del paquete distribuible. La captura demuestra que una pose tiene una silueta más razonable; todavía no constituye validación cuadro por cuadro contra Acknex.

## Estado verificado

- Prueba de pivote Q16.16: PASS C89 (90° alrededor de P=(10,0,0)).
- Corpus auténtico Tusker, fotograma 30: 69 huesos, 1.921 vértices ponderados, 5.953 influencias.
- Total de dimensiones del AABB tras skinning: interpretación anterior 26.673.537 Q16 vs nueva 13.588.527 Q16 (aprox. 407 vs 207 unidades).
- Muestra real Tusker: repetir la pose produce los mismos vértices; prueba de regresión con Clang ASan+UBSan PASS.
- Biblioteca: GCC C89, Clang C89 ASan+UBSan, build Legacy-only y MDL7-only PASS.
- Pruebas de integración: fases 07, 08, 09, 10, 11, 12, 13 y 14 PASS.
- Capturas VTK: Tusker fotogramas 1, 30, 60; Goddess4arms fotograma 18; NinjaGirl fotograma 60.
- Todavía sin referencia visual de ejecución Acknex; animaciones y combinación de clips model-space requieren validación de matrices por muestra.
- Windows MinGW32 no ejecutado en este entorno.

Referencia técnica de matrices de offset / pivotes: https://github.com/assimp/assimp/issues/2773 ; https://www.cs.cornell.edu/courses/cs5625/2022sp/assignments/animation.html
