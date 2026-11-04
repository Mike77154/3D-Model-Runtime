# Source SDK / Licker / Hunter: diagnóstico comprobado — v1.0.15

## Hallazgo en Source 2013

En `src/public/bone_setup.cpp`, `CalcBonePosition()` toma `ExtractAnimValue(..., baseBoneScale[j], ...)` en `STUDIO_ANIM_ANIMPOS` y lee `Vector48` en `STUDIO_ANIM_RAWPOS`. Cuando los archivos traen escalas o valores absurdos, el SDK **no** cambia automáticamente esos valores por poses plausibles. `pLinearBones` lee la tabla de escalas paralela cuando existe. Se inspeccionó la tabla real del MDL de Licker y los valores erráticos coinciden también allí.

### Prueba independiente de los bytes del corpus

* `licker.mdl` Source 49 / `@att1`, fotograma 74: `Tongue5` posee `posscale.z ≈ 4.168168993849151×10^21` y un canal RLE con muestra `32766`; el producto matemático fuente es ≈ `1.3657422525246128×10^26` unidades, **fuera del alcance de Q16.16**. No es un error de orden de matrices corregible con IK.
* `reo_hunter.mdl` Source 48 / `@ragdoll`: el `RAWPOS` del hueso raíz contiene valores binary16 `-65472` y `-65504`. Son valores en el MDL y no una supuesta traslación física ordinaria.

Esto demuestra **anomalías dentro de los archivos Source portados**. No permite concluir que un videojuego original contenga tales errores: el archivo es una adaptación de Workshop.

### Implementación

Nueva entrada `hl2m_master_anim_guard89()` en el master. Al utilizarla explícitamente, el decodificador repone únicamente las coordenadas locales cuya diferencia respecto de la pose de referencia supera un umbral elegido por el llamador. Las rotaciones, posiciones válidas y demás vértices permanecen intactos. Devuelve número de huesos afectados, ejes reemplazados e índice del primer hueso. Se evita el desbordamiento con suma entera saturada.

El modo **no reproduce literalmente Source**: es una herramienta de recuperación visual, no solución de datos originales. La API normal `hl2m_master_anim89()` conserva la decodificación sin ese rescate.

### Pruebas (umbral de 512 unidades Source)

| Archivo | Muestras examinadas | Reemplazos | Errores de lectura |
|---|---:|---:|---:|
| Licker RE3R port | 64 | 9 muestras | 0 |
| REO Hunter | 4 | 3 muestras | 0 |
| T-103 fase 1 | 128 | 0 muestras | 0 |

- Licker `@att1` fotograma 74: 31 huesos y 91 ejes corregidos; máximo elemento de matriz world antes: `2,087,679,662` Q16.16; después: `10,597,476` Q16.16.
- Licker: caja de vértices raw (cuadro 74) x[-28468,34], y[-29379,19], z[-21732,14986] unidades; con recuperación x[-27,165], y[-33,19], z[-0.82,99] unidades. **La lengua del modelo de referencia sigue siendo larga; no está recortada por el guard.**
- Hunter `@ragdoll` fotograma 0: un hueso, dos ejes; matriz máxima corregida 2,403,237 Q16.16. Este ragdoll no puede validarse como ataque o caminata real.
- T-103 `Walk_F`: no hubo sustituciones; se conservó la pose nativa.
- `make check` con `-std=c89 -Wall -Wextra -Werror -pedantic -O2`: aprobado, incluido `test_anim_guard115` (RLE desmesurado, RLE correcto sin cambios y RAWPOS anómalo).

## Limitaciones que siguen abiertas

El rescate no reconstruye los **datos auténticos que pudieron perderse durante el port**. No se ha demostrado la equivalencia exacta de todos los ataques de Licker con el Source runtime ni con los videojuegos de origen. Siguen pendientes interpolación de poses continuas, mezcla completa de secuencias, detalles de `RAWROT` y materiales/shaders de GPU en el visor.

Archivos de corpus usados para pruebas, no incluidos en el ZIP del código.
