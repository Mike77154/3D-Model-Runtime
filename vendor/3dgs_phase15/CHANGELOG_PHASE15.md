# Fase 15 — MDL7 animation matrices, direct file-space path

- Hallazgo del corpus **Tusker.mdl**: las matrices de animación poseen traslaciones compensatorias del pivote (P−RP), por ejemplo en huesos de columna/cabeza. La composición adicional de T(rest) y la cadena de padres producía estiramientos en el frame 30.
- **Nuevo modo experimental y optativo**: `gs3d_set_pose_mode(GS3D_POSE_FILE_SPACE)` usa las matrices afines de cada hueso tal como se decodifican del fichero. No suma las posiciones de reposo una segunda vez.
- El modo anterior `GS3D_POSE_HIERARCHY` continúa por defecto para evitar romper los tests y los consumidores existentes.
- `vtk_capture.py --animate` usa por defecto el modo file-space y permite `--pose-mode legacy` para comparar ambos; sin OBJ/FBX/GLB de intermediario.
- No se declara equivalencia completa con MED/Acknex: falta validar algunas convenciones y las interpolaciones TRS cuando las claves ya son matrices con pivotes incorporados.
- C89 estricto, Q16.16, sin malloc/calloc/realloc/free/float/double/long long en los módulos nuevos.

### Real-corpus validation
- Tusker.mdl, 17,586,402 bytes: `test_phase15_real_corpus.c` comprobó las 5.953 influencias explícitas, la forma bbox y la idempotencia de poses.
- `tests/test_phase15_matrix_space.c` cubre el caso con pivote global compensado en el registro de matriz.
- `scripts/check_phase15.sh` genera los fixtures sintéticos antes de invocar pruebas; opcionalmente ejecuta el test real con `GS3D_REAL_TUSKER=/ruta/Tusker.mdl`.
- Los demás corpus de referencia no contienen la misma densidad de pesos que Tusker. Goddess y Ninja tienen capturas de pose, sin garantía de conformidad con el runtime original.
