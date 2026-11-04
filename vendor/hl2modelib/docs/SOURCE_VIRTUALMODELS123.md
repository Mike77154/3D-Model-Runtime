# hl2modelib 1.0.23 — Source virtualmodel, múltiples bancos

## Resultado y referencia del SDK

Comparación realizada directamente con `source-sdk-2013-master/src/public/studio_virtualmodel.cpp`: `AppendModels`, `AppendSequences`, `AppendAnimations`, `AppendBonemap` y `AppendPoseParameters`. Se conservó el soporte anterior, y la ampliación es un vendor independiente C89.

| Función | Implementación v1.0.23 |
|---|---|
| `$includemodel` | 1 MDL base + hasta 3 bancos directos, suministrados explícitamente |
| Secuencias | ID virtual único, deduplicación por etiqueta y `STUDIO_OVERRIDE` |
| Animaciones | Tabla por nombre y mapa local→virtual por banco |
| Huesos | Mapa por nombre al esqueleto base, comprobación de jerarquía |
| Pose parameters | Unifica rangos homónimos, normaliza a rango de cada MDL |
| ANI externos | Acepta los datos del banco que los necesita, suministrados por el motor |
| Playback | Selección del grupo original → evaluador v1.0.21 → remapeo a huesos base |
| C89 | Sin `malloc`, `free`, `float`, `double` ni `int64_t` |

El catálogo es propiedad del llamador y conserva **punteros no propietarios** a sus MDL y ANI. No debe destruirlos mientras lo use. Las nuevas funciones son optativas y las anteriores permanecen intactas.

## Verificaciones ejecutadas

1. `test_virtualmodel123`: **PASS**. Tres grupos sintéticos, índices virtuales, override, unión de rangos físicos, remapeo de huesos y errores no destructivos.
2. `hl2m_virtual_probe123 re_zombie01.mdl mbczom_anims.mdl`: **PASS**. `39` secuencias, `39` animaciones, `1` controlador global, `41` huesos relacionados de `53` en el personaje; cuatro secuencias heredadas `Idle01`–`Idle04` devuelven una pose correctamente.
3. `hl2m_virtual_real_twobank123` sobre una **copia modificada en memoria** del zombi y dos referencias a bytes del banco real: **PASS**. Tres grupos, `40` secuencias, muestra del grupo 2 con 53 huesos. *Ensayo instrumental, no un MDL multibanco original.*
4. Corpus `massive resident NPC hl2.zip`: **nueve MDL con dos includes directos**, entre ellos `re3nemesis_normal.mdl`, que declara `models/Police_Animations.mdl` y `models/Police_ss.mdl`. **Los bancos no están incluidos en ese ZIP**, así que no se pudo reproducir su animación heredada ni comparar visualmente su fidelidad.
5. `make check` bajo `-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude`: PASS en el árbol fuente actualizado, incluida toda la suite heredada.

## Código nuevo

- `vendors/source/animator/hl2m_virtualmodel123.c` — motor de catálogos y remapeo multibanco.
- `include/hl2modelib.h` — tipos `hl2m_virtual_input123`, `hl2m_virtual_multi123`.
- `include/hl2m_master89.h` y `vendors/source/master/hl2m_master89.c` — entrada `hl2m_master_virtual_sequence_multi123`.
- `tests/test_virtualmodel123.c` — prueba reproducible sin assets.
- `examples/hl2m_virtual_probe123.c` — consulta de dependencias y banco único/múltiple real.
- `examples/hl2m_virtual_real_twobank123.c` — prueba de segundo banco controlada con corpus real, no incluido.

## Uso mínimo

```c
/* caller already decoded 1 base + N source MDL and owns all buffers */
hl2m_virtual_input123 groups[3];
hl2m_virtual_multi123 catalog;
hl2m_pose_matrices pose;
hl2m_sequence_layers_report120 report;
/* ... initialize paths/MDL bytes/optional ANI bytes and mdl pointers ... */
if (hl2m_virtual_build_multi123(groups, 3u, &catalog) == HL2M_OK) {
    /* Provide normalized controllers sized to catalog.pose_count. */
    hl2m_master_virtual_sequence_multi123(&catalog, virtual_sequence,
        cycle, controllers, (unsigned int)catalog.pose_count, 0, &pose, &report);
}
```

## Qué falta para paridad completa con Source 2013

Los grupos incluidos de forma **recursiva**, autolayers que hacen referencia a otros grupos, `SRC bone transforms`, remapeos de la jerarquía cuando difiere y el IK avanzado de Source no están implementados. Los vínculos incompletos se rechazan deliberadamente, mientras que el motor original puede ignorar un include ausente. Ni las 39 motions del zombi ni la prueba controlada constituyen una comparación píxel a píxel o fotograma a fotograma frente al ejecutable Source.
