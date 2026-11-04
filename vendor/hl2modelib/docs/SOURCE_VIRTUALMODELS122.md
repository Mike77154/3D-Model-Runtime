# hl2modelib v1.0.22 — Source virtualmodel_t: primer remapeo reproducible

## Comparación con el SDK de Valve

Referencia estudiada, no copiada: `source-sdk-2013-master/src/public/studio_virtualmodel.cpp` y `src/public/studio.h`; `bone_setup.cpp` para la aplicación de `boneMap`/`masterBone`.

| Regla original | Nueva implementación C89 |
|---|---|
| `studiohdr_t::numincludemodels` / `pModelGroup` | Lee entradas de 8 bytes y rutas relativas, validando el span y la terminación NUL |
| `AppendBonemap` | Correspondencia de huesos por nombre sin distinguir mayúsculas, `child_to_base_bone` y `base_to_child_bone` |
| `AppendSequences` | Catálogo virtual con deduplicación por etiqueta, prioridad de `STUDIO_OVERRIDE` |
| `AppendAnimations` | Catálogo virtual con nombres y remapeo de índices locales |
| `AppendPoseParameters` | Fusión de controladores homónimos y rango físico total |
| `RemapAnimBone` / `CalcAnimation` | Remapeo optativo de transformaciones locales al esqueleto del personaje base; reconstrucción de matrices mundiales |

**Alcance exacto:** solo dos grupos (modelo principal más UN modelo incluido por vinculación). Los recursos son entregados explícitamente por el motor cliente, sin acceder al disco dentro del códec. No se implementaron los grupos múltiples recursivos, los remapeos de blends aditivos entre grupos, el sistema IK completo, los `SRC bone transforms` ni las poses `world/local` especiales. El catálogo es una fase real de soporte de `virtualmodel_t`, **no compatibilidad idéntica con Source**.

El remapeo de matrices rechaza diferencias en la jerarquía ósea, en vez de aplicar un arreglo visual no comprobado. Esta decisión es más conservadora que Source, que puede registrar advertencias y continuar. El modo anterior de secuencias y el resto de los vendors conservan sus API.

## Prueba con modelos RE reales

Corpus: `massive resident NPC hl2.zip` (excluido HOD2).

| Dato | `re_zombie01` + `mbczom_anims` | `reo_hunter` + `anim_hunter` |
|---|---:|---:|
| Versión MDL Source | 48 | 48 |
| Huesos base/incluido | 53/56 | 53/53 |
| Correspondencias óseas | 41 | 53 |
| Conflictos de jerarquía | 0 | 0 |
| Secuencias virtuales | 39 | 91 |
| Animaciones virtuales | 39 | 166 |
| Parámetros virtuales | 1 | 15 |
| Pose heredada comprobada | `Idle01` | `Idle_Upper_KNIFE` |
| Secuencia exportada con skinning | `Idle01` | `Run_Upper_Knife` |
| Triángulos por fotograma | 4,449 | 2,998 |
| Influencias óseas faltantes | 0 | 0 |
| Archivo ANI externo | No | Sí, `anim_hunter.ani` |

Se exportaron OBJ en los ciclos 0, 0.25, 0.5 y 1.0 de ambas secuencias. Las posiciones de los vértices **cambiaron**, y los límites espaciales permanecieron razonables. Se generó una galería **sin texturas**, desde el OBJ real, mediante un presenter externo VTK. Esto confirma el funcionamiento de las dos rutas seleccionadas; **no garantiza todas las secuencias ni la paridad con Source ejecutable**.

## Pruebas sintéticas y de regresión

- Rutas MDL `models/foo/bar.mdl`, normalización de `\\` a `/`, coincidencia sin distinguir mayúsculas.
- Tablas relativas inválidas y referencias externas no declaradas se rechazan.
- Duplicación de secuencias y prioridad de bandera `STUDIO_OVERRIDE`.
- Remapeo de huesos con índices distintos; extra bones no alteran la tabla base.
- Jerarquía incompatible devuelve error y conserva intacta la pose de salida.
- Un parámetro `aim_yaw` con rango base ±90° y rango incluido ±180°: control virtual 0.75 produce control local base 1.0 y control local incluido 0.75.
- Compilación completa con `-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude` desde fuente y `make check` con todas las pruebas heredadas.

## Interfaces nuevas

Declaradas en `include/hl2modelib.h`:

- `hl2m_virtual_includes122` — lee rutas de `$includemodel`.
- `hl2m_virtual_build122` — construye índices, catálogo y mapa de huesos.
- `hl2m_virtual_base_controls122` y `hl2m_virtual_child_controls122` — valores normalizados globales a locales.
- `hl2m_virtual_remap_pose122` — reconstruye una pose del esqueleto base a partir de un banco.

En `include/hl2m_master89.h`:

- `hl2m_master_virtual_sequence122` — selecciona la procedencia de una secuencia virtual y la evalúa con el player existente de v1.0.21. Requiere buffers MDL/ANI del grupo apropiado y el vínculo ya construido.

Herramientas: `hl2m_virtual_probe122` y `hl2m_virtual_obj122` (ambos portables C89). Se añadió `tests/test_virtualmodel122.c` y la prueba a `make check`.

## Próxima compatibilidad pendiente

Soporte de más de un modelo incluido en un mismo catálogo (p. ej., Nemesis con dos bancos `Police_Animations` y `Police_ss`), capas que referencian secuencias de otros grupos y `pose-to-bone`/transformaciones correctivas entre esqueletos diferentes. Esas rutas precisan verificar metadatos y los efectos sobre la geometría con modelos reales.
