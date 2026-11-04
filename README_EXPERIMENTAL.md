# Modelo Runtime Experimental — Wave 5 (45 providers)

Consulta `NATIVE_ONLY_WAVE5_STATUS.md` y `EXPERIMENTAL_PROVIDER_STATUS.csv` para conocer los 10 adaptadores nuevos y sus límites.

# Modelo Runtime Experimental — 35 proveedores nativos

**Sin GACKT-LANG ni representación cooked obligatoria.**

- 14 proveedores históricos de `ModelRuntime89 v19` preservados.
- 21 proveedores vendorizados de GACKT realmente enlazados con visitas nativas (6 nuevos en Wave 4).
- 44 vendors adicionales conservados como fuentes, todavía sin adaptadores operativos.

Lee `NATIVE_ONLY_WAVE4_STATUS.md` para soporte comprobado, limitaciones y resultados, y `EXPERIMENTAL_PROVIDER_STATUS.csv` para todos los vendors. Documentación de las rondas anteriores permanece en el paquete.


## Latest: corrected Native Tanda 03 (65)
See `NATIVE_TANDA03_REAL_STATUS.md`. This package contains 10 *actually linked* provider interfaces beyond the 55-provider checkpoint. The earlier 65-label stub release was not an operative integration.


**Latest checkpoint: 70 native registrations.** See `NATIVE_BATCH8_CHECKPOINT.md` for tested changes and explicitly unresolved work. Older 65-provider text is historical.


## Native 75 IQM dense-frame update
IQM v2 supports authored frame-indexed native **local bone translations** (101 frames of mrfixit.iqm checked), with strict C89 32-bit arithmetic. The mesh is still bind-pose: no quaternion pose composition / vertex skinning. See NATIVE75_IQM_FRAME_PATCH.md and tests/native_iqm_frames89_regression.c. Provider count stays 75.

## Latest continuation: IQM native quaternion + skinning

Provider 101 (IQM) now supports quaternion frame transforms, parented skeletal matrices and CPU-skinned vertices. See `NATIVE75_IQM_SKINNING_PATCH.md` and `include/modelrt_native_iqm_pose89.h` for its additional native pose interface and limitations. **75 registered providers, unchanged.**
