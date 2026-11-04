# Prueba de corpus Licker RE3R + ZombieStandard (Source)

## Clasificación verificada

**Licker:** `lickerRE3R.zip` contiene un único archivo `temp.gma` (addon de Garry's Mod llamado `drg_re3licker`), con `models/roach/re3/licker.mdl` Source IDST 49, `licker.vvd` IDSV 4, tres variantes VTX v7 (`dx80`, `dx90`, `sw`), dos VMT VertexLitGeneric, cuatro VTF, archivos Lua y sonidos MP3. No es MDL GoldSrc/HL1. La denominación RE3R figura en los nombres del addon; el contenedor no acredita por sí solo la extracción original del juego.

**Zombis:** `zombiestandart.rar` no contiene MDL compilados en su nivel principal; contiene 1.508 SMD, 32 QC, 25 escenas SCN, 9 QCI, material de trabajo y archivos anidados. Los SMD son texto `version 1`, por sí solos no distinguen HL1/HL2; los QC sí muestran comandos de **Source**: `$poseparameter`, `$includemodel player/cs_player_shared.mdl`, `$ikchain`, `$collisionjoints`, etc. Un `materials.rar` interno contiene 42 VMT, 102 VTF y 102 TGA. Algunos QC refieren animaciones/modelos compartidos que no necesariamente vienen dentro del paquete.

## Tests realizados

| Prueba | v1.0.12 sin cambios | Build experimental |
|---|---|---|
| Licker MDL 49 | MDL OK | OK, 123 huesos, 22 animaciones, 22 secuencias |
| Licker VVD | error -5, `HL2M_ERR_LIMIT`, 41.704 > 32.768 | OK con 65.536 de capacidad |
| Licker VTX dx90 | no llegaba a mesher | OK, 72.889 triángulos, 4 draws, 0 descartados |
| Licker dx80/sw | sin probar antes del bloqueo | ambos ensamblan en la prueba ampliada |
| Licker VMT | OK | bump, Phong, detail declarados |
| Licker VTF | OK | dos albedos y dos normal maps, 1024x1024 en la muestra |
| MalePunk SMD | malla OK, UV equivocadas | 3.654 triángulos, 2 materiales, UV recuperadas |
| FemaleHobo SMD | malla OK, UV equivocadas | 3.984 triángulos, 2 materiales, UV recuperadas |
| Regresiones heredadas | no reejecutadas en esta sesión | `make check` con C89, `-Werror -pedantic`: PASS |

## Correcciones del build experimental

1. `HL2M_MAX_VVD_VERTS` pasa de 32.768 a 65.536.
2. Los límites del modelo de dibujo pasan a 262.144 vértices y 262.144 índices, **estáticos**, sin malloc ni 64-bit. Esto incrementa RAM fija.
3. Parser decimal de SMD/QC: sustituye `(frac * 65536)/scale` (desbordamiento de entero con seis cifras decimales) por división binaria iterativa de 16 pasos con operandos <= 2.000.000.
4. El SMD mesher copia el nombre de material a cada draw, para que el texturizador tenga `MalePunkBody.tga`, `MalePunkHeadA.tga`, etc.

**Importante:** es una compilación experimental con código fuente, no se ha marcado como versión estable 1.0.13. No se verificó aún la reproducción integral de sus animaciones ni la paridad de los shaders de GMod. La comparación visual de Licker usa texturas albedo con alfa forzado opaco solo en el renderer de diagnóstico, porque el VMT usa `$basemapalphaphongparts` y el canal alfa no debe interpretarse automáticamente como translucidez.

## Datos verificables

Licker: MDL id `IDST`, v49, 123 huesos, 22/22 animaciones y secuencias. VVD v4 con 41.704 vértices. Malla exportada desde MDL+VVD+VTX: 218.667 vértices **de dibujo duplicados por triángulo**, 72.889 triángulos y cuatro grupos, no son 218.667 vértices únicos.

MalePunk: 10.962 vértices de dibujo, 3.654 triángulos, SMD con 36 huesos; FemaleHobo: 11.952 vértices de dibujo, 3.984 triángulos, 36 huesos. Ambas importaciones mapean albedo VTF auténtico. Las dos imágenes son de un visualizador ajeno (VTK), alimentado por geometría que exportó hl2modelib. No se ha demostrado todavía que las motions funcionen en tiempo real.
