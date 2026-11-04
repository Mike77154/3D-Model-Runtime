# Fase 12 — Compatibilidad con el corpus real 3D GameStudio MDL7

Se trabajó sobre el código completo de Fase 11, sin reconstruir desde cero. El código nuevo de la biblioteca sigue siendo ISO C89 estricto, sin `malloc`, `calloc`, `realloc`, `free`, `float`, `double` ni `long long`.

## Fallos corregidos

1. **MDL7 de 26 bytes por triángulo:** las entradas UV2 `0xFFFF` son legítimas cuando la segunda referencia de material es `0xFFFFFFFF`. Se siguen rechazando índices UV2 inválidos si la segunda capa está activa.
2. **Ubicación alternativa de deformadores:** los modelos sintéticos anteriores guardaban bloques al final de cada `groupdata_size`; en `Tusker.mdl` el bloque real de deformación está **después de todos los grupos** y antes de `data_size`. El lector acepta ambos layouts después de validar los límites; no confunde estos bytes con el lump MED.
3. **Límite de pesos ampliado:** la capacidad predeterminada pasa de 4 a 12, límite configurable entre 1 y 16. Esto admite el máximo **declarado de 10** del archivo real de Tusker sin eliminar influencias.
4. **Auditor:** muestra además el número de vértices que recibieron pesos.

## Hallazgo: Tusker

El archivo `Tusker.mdl` aportado por el usuario declara 69 huesos, 139 fotogramas, 4 skins, 1 deformador, 10 pesos máximos por vértice. Su bloque fuera de grupo mide 49.516 bytes y contiene 67 elementos con **5.953 influencias sobre 1.921 vértices**. Es un corpus real muy útil para revisar la reconstrucción de skinning.

## Corpus real verificado con el lector y exportador

| Modelo | Vértices | Caras | Huesos | Frames | Skins | Influencias explícitas |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LadyJ | 2377 | 2016 | 26 | 1 | 1 | 0 |
| ShinobiJ | 1759 | 1860 | 27 | 1 | 1 | 0 |
| NinjaGirlShinobi1 | 410 | 816 | 27 | 120 | 1 | 0 |
| Goddess4arms | 2625 | 2580 | 44 | 37 | 1 | 0 |
| Tusker | 1921 | 3732 | 69 | 139 | 4 | 5953 |
| TuskerSpear | 45 | 86 | 69 | 139 | 4 | 0 |

`Goddess4arms.mdl` contiene además un lump MED de 160.756 bytes, mantenido fuera del flujo de mallas. Las vistas previas estáticas se rasterizaron a partir de OBJ/TGA exportados por el códec, no de imágenes generadas.

## Validación

- Código de biblioteca: GCC `-std=c89 -pedantic -Wall -Wextra -Werror`.
- Test específico Fase 12 con AddressSanitizer y UBSan: **PASS**.
- Regresiones de geometría, morphs, pesos y animación Fases 01–10: **PASS** en ejecuciones individuales; Fase 11 PASS en ejecución individual.
- Seis archivos MDL7 reales: apertura y exportación OBJ/MTL/TGA **PASS**.
- Registros de pesos auténticos de Tusker: **lectura y validación de límites PASS**.
- Resultado exacto del skinning de matrices de Acknex frente al motor comercial: **NO VERIFICADO**. Las vistas previas de poses animadas son experimentales.
- Compilación Win32/MinGW32 auténtica: **PENDIENTE**.

## Puentes inspeccionados

- FragFormats_C89_R9_SOURCE_ONLY: documentación de estructuras MDL7, matrices en crudo y chequeos de pesos; sirve de referencia independiente.
- AcknexWrapper 8.0.3.2: APIs `ent_animate`, `ent_animatefrom`, `ent_blend`, `ent_bonemove`, `ent_bonerotate` y propiedades de poses; describe semántica de motor, no es un parser binario para nuestro runtime.
- t7: fuentes de plantilla para el motor; no aporta por sí solo una especificación completa del layout binario MDL7.

**Nota:** los archivos comerciales del corpus y los originales de terceros no se redistribuyen en este paquete *source-only*. El usuario los conserva por separado.
