# v1.0.24 — Modelo virtual recursivo y capas compartidas

## Referencias del SDK

- `source-sdk-2013-master/src/public/studio_virtualmodel.cpp`: `virtualmodel_t::AppendModels`, `AppendSequences`, `AppendAnimations`, `AppendBonemap`, `AppendPoseParameters`.
- `source-sdk-2013-master/src/public/bone_setup.cpp`: `CBoneSetup::AddSequenceLayers`, `GetSharedPoseParameter`, `iRelativeSeq`, `AccumulatePose`.

El código de v124 es una implementación C89 propia y limitada, NO una copia completa del motor de Valve.

## Contrato y compatibilidad

- Se conserva `hl2m_virtual_build_multi123`: valida que TODOS los bancos sean inclusiones directas del MDL raíz.
- `hl2m_virtual_build_recursive124(inputs, count, &catalog, &graph)` acepta bancos en orden arbitrario, y determina orden DFS. `inputs[0]` representa el MDL base; cada hijo requiere `include_path` igual al nombre en el `$includemodel`. Los bytes y los MDL decodificados son propiedad del motor cliente y deben permanecer vivos.
- El grafo rechaza ruta duplicada, ciclo real, hijo inexistente, entrada huérfana, más de cuatro grupos. Compartir un banco por dos ramas utiliza un único grupo. Un fallo no cambia `catalog` ni `graph`.
- `hl2m_virtual_layer_target124` consulta el mapeo `masterSeq` del grupo propietario de la secuencia. Un `STUDIO_OVERRIDE` puede hacer que la secuencia resuelta pertenezca a otro banco.
- `hl2m_master_virtual_sequence_layers124`: evaluación opcional de secuencia principal + capas recursivas entre grupos con `iRelativeSeq`, pesos heredados y máscaras proyectadas al esqueleto de raíz. `STUDIO_AL_POSE` usa el parámetro del grupo OBJETIVO. Protege contra recursión circular e invalidez de jerarquías. La salida es transaccional.
- `HL2M_VIRTUAL_GROUP_CAP123` sigue siendo 4. Al conservar las estructuras y funciones previas, no hay reemplazo de ABI obligado para consumidores de las APIs antiguas si recompilan/enlazan esta biblioteca.

## Reproducción

```sh
make check CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude'
make hl2m_virtual_probe124 hl2m_virtual_override_layer_probe124 CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude'
./hl2m_virtual_probe124 re_zombie01.mdl 'models/mbczombies/mbczom_anims.mdl=mbczom_anims.mdl'
./hl2m_virtual_probe124 reo_hunter.mdl 'models/infected/anim_hunter.mdl=anim_hunter.mdl'
./hl2m_virtual_override_layer_probe124 reo_hunter.mdl anim_hunter.mdl anim_hunter.ani
```

Los modelos RE originales NO se incluyen en el archivo de código.

## Pruebas efectuadas

1. `test_virtualmodel124`: 4 MDL sintéticos (base→A→B, base→C→B), entradas desordenadas, deduplicación en diamante, `STUDIO_OVERRIDE`, resolución de capa a banco distinto, rechazo de ciclos, faltantes, huérfanos y duplicados: PASS.
2. RE `re_zombie01` y `mbczom_anims` originales: 39 secuencias compartidas, pose de 53 huesos, PASS. Sin capas activas en esa muestra.
3. REO Hunter con `anim_hunter.mdl` y `anim_hunter.ani` reales: 91 secuencias virtuales, 166 animaciones, `Pounce_01` con 1 autolayer aplicada, 53 huesos: PASS.
4. Ensayo controlado *en memoria* de REO Hunter: una secuencia del modelo principal se declaró `STUDIO_OVERRIDE` y se creó una referencia autolayer para redirigirla a `Pounce_01` del banco incluido. Se evaluó con la animación original y produjo dos capas acumuladas y una pose de 53 huesos diferente de la pose base. **No es una propiedad original de ese modelo de workshop.**
5. `Idle_Upper_KNIFE` del Hunter: el material original usa un overlay DELTA que actualmente dispara `HL2M_ERR_UNSUPPORTED` en la validación; no se considera aprobado visualmente ni se fuerza una composición incorrecta.

## Límites

No implementados: `STUDIO_AL_LOCAL`, secuencias WORLD, máscaras y reglas de IK del motor, parámetros de secuencias virtuales con todos los casos límite, modelos >4 grupos, ni equivalencia validada contra imágenes del motor Source en ejecución. Las pruebas sintéticas del grafo NO representan un banco recursivo real del corpus.
