# RE3 Codec v0.18.0 — morphs y bancos de daño (experimental)

**Estado:** compilación GCC y Clang validada en Linux. MinGW32 no fue ejecutado aquí.
**Base:** códec v0.17, corpus EMD/TIM aportado por el usuario. La distribución de código no incluye recursos del juego.

## Cambios implementados

1. `re3c_animated_body_vertex_morph(...)`: renderiza vértices de un fotograma morph explícito usando DAT, seams y matrices del modelo activo. `RE3C_MORPH_REST` conserva la ruta anterior byte a byte. Las pistas sin morph conservan la geometría base; frames fuera del rango retornan `RE3C_ERR_RANGE`.
2. `re3c_retarget_pose_explicit(...)`: transforma rotaciones entre bancos según un mapa suministrado explícitamente; no asume que dos esqueletos con igual número de articulaciones sean equivalentes. Usa copia temporal para validar por completo antes de escribir la pose destino. `RE3C_JOINT_NEUTRAL` deja una articulación sin rotación de la fuente.
3. `re3c_build_joint_matrices(...)`: evita matrices basadas en bancos sin jerarquía (`hierarchy_source==255`), que antes podían ser matrices de articulaciones cero. No se fabrican posiciones ni padres para los bancos de daño sin rig.
4. `re3emd_anim_preview`: añade dos parámetros opcionales después de los argumentos v0.17: `rest|morph:N` y `no-map|zombie15to11|nemesis15to16`. Los dos mapeos sólo funcionan en **banco 2 daño** y están marcados como experimentales; el valor predeterminado continúa siendo seguro.
5. `re3emd_morph_audit`: barre todos los frames morph de cada pista del EMD y evalúa todos sus vértices con skinning animado. `test_morph_retarget` añade pruebas sintéticas, controles de límites y comparaciones sobre `EM36.EMD` real si se le pasa como argumento.

## Mapeo 15 -> 11 de zombis: evidencia sólida del esqueleto destino

Comparación directa del esqueleto normal `EM10` (15 articulaciones) contra `EM16`, `EM18`–`EM1F` (11 articulaciones). Las 11 articulaciones del segundo conjunto coinciden *exactamente* con un subconjunto del primero, tanto en **posición mundial** como en **jerarquía de padres**:

`destino(11) <- fuente(15) = [0,1,2,3,5,6,8,9,10,12,13]`

Eso fundamenta una hipótesis de retarget para las rotaciones de daño, pero **no prueba** que la codificación de los 15 joints en cada banco de daño tenga la misma semántica que el rig normal `EM10`. Tampoco prueba coincidencia temporal con el motor original. Por eso el remapeo sólo se habilita mediante `zombie15to11`.

## Mapeo Némesis 15 -> 16: sólo candidato visual

`EM36` tiene 16 articulaciones normales y 15 rotaciones en el banco de daño, sin rig físico heredable. Para visualizar, se probó el mapa `[0, NEUTRAL, 1, 2, ..., 14]` sobre el rig normal; esto es un **esquema hipotético** que necesita contrastarse con el motor original. Ninguna herramienta lo activa por defecto.

## Morphs de EM36

- 8 pistas en total, 53 frames morph sumados, de las que 2 pistas afectan a partes normalmente visibles.
- El estado morph `1` modifica 258 vértices animados frente al estado sin morph, en el primer fotograma del clip normal 1.
- El barrido completo de las 8 pistas de `EM36` evalúa 1,242 vértices (con repetición entre frames); 1,018 posiciones difieren respecto al estado base.
- La silueta de cabeza/torso cambia notablemente en la vista previa, pero **no hay prueba** de que el juego active ese estado en ese clip y momento. No se atribuyen causas narrativas (decapitación, mutación, etc.) sin evidencia.

## Regresión

- Carga estructural: **100/100 EMD**.
- Prueba de texturas: **100/100 EMD+TIM**.
- Animaciones: **100/100 EMD** (`test-animation-corpus`).
- Morphs: **100/100 EMD**, 50 pistas, 273 frames morph, 8,933 transformaciones de vértice, 6,962 cambios respecto a geometría base. **12 archivos** del corpus tienen morphs; otros 88 no los tienen.
- C89 estricto: GCC y Clang; tests sintéticos y de morph: PASS.
- GCC UBSan: tests y render de morph de EM36 sin errores observados.

Los 100 archivos incluyen variantes del mismo nombre en `EMD/` y `EMD08/`; no equivalen a 100 modelos narrativamente distintos.

## Comandos (Linux/MSYS2 MinGW32)

```sh
make -j2
make test
./test_morph_retarget /ruta/a/EM36.EMD
mkdir -p salida_morph
./re3emd_anim_preview /ruta/a/EM36.EMD /ruta/a/EM36.TIM salida_morph 0 1 12 front texture model main morph:1

mkdir -p salida_danio
./re3emd_anim_preview /ruta/a/EM16.EMD /ruta/a/EM16.TIM salida_danio 2 0 12 front texture model main rest zombie15to11
```

**Limitaciones pendientes**: selección temporal de morphs a partir de bytecode/SCD, correspondencia verdadera de rigs de daño en el ejecutable de PC, estados de partes opcionales, colisiones, temporización/fps, y fidelidad del renderizado respecto al juego.
