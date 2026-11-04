# hl2modelib v1.0.14 — Auditoría Resident Evil (animación y texturas)

## Alcance

Datos originales examinados: Massive Resident NPC HL2, Chronicles Package HL2, T-103, Licker RE3R, Ustanak, Hunter β, RE5 Model Pack, RE0/REmake Model Pack. **Se excluyeron los paquetes de House of the Dead y los SMD de Land of the Dead** por no ser corpus Resident Evil MDL. El paquete RE0/REmake contiene duplicados idénticos de Chronicles.

Se examinaron **146 MDL** (128 contenido único SHA-256), con **2329 declaraciones de animación** (1913 únicas en los 128 MDL deduplicados). Para cada animación se solicitó al códec una pose al inicio, otra a mitad y otra al final, con una ejecución total de **6987 muestras**, todas con código de retorno 0. Esto valida **lectura de matrices**, NO fidelidad a los FPS nativos, IK avanzado, sequence blending o correcto skinning de cada fotograma. Parte de las animaciones de un solo fotograma y los ragdolls son esencialmente poses estáticas. Las pruebas no los confunden con una caminata.

Cuatro bancos (`anim_charger.ani`, `anim_hunter.ani`, `anim_smoker.ani`, `zombie_soldier_animations.ani`) aportan **476 animaciones externas** que antes eran `HL2M_ERR_UNSUPPORTED`. La v1.0.14 acepta ahora un buffer ANI desde el master codec y evalúa sus canales. Las animaciones seccionadas almacenadas dentro del MDL usan el fotograma local calculado como `mstudioanimdesc_t::pAnim()` en Source SDK, con caso especial para el último fotograma.

### Estabilidad geométrica

Se analizaron matrices en tres muestras por animación. Criterio de aviso: coeficiente de rotación mayor de 3 o traslación mundial mayor de 2048 unidades Source. Cuatro motions quedaron marcadas:
- `licker.mdl` RE3R: `@att1`, `@att2`, `@climb_3`.
- `reo_hunter.mdl`: `@ragdoll`.

En `licker.mdl` la pose bind es plausible (máximo absoluto ~185 unidades), pero las deformaciones de algunos ataques alcanzan ~30 000 unidades en la malla, sobre todo por transformaciones de huesos Tongue5…; algunos canales acaban saturando Q16.16. Esta versión **NO repara** esos datos arbitrariamente y **NO certifica** las 22 motions del Licker como visualmente correctas.

### Texturas

Se leyeron los VTF comprimidos de los corpus ZIP originales: 864 entradas, de las que 16 eran bytes repetidos; **848 VTF únicos** pudieron decodificarse hasta RGBA con el decodificador de la biblioteca y sin errores. El inventario no prueba automáticamente paridad completa de VMT, transparencia, mapas de normales, Phong, iluminación GPU, mipmap/aniso ni rutas de todos los materiales sobre cada criatura.

Se exportaron tres fotogramas por modelo de Lisa Trevor, Hunter β, Ustanak, El Gigante, Wesker, Licker, Nemesis y T-103 (24 exportaciones de prueba); adicionalmente se generaron 4 GIF de 9 fotogramas muestreados para T-103, Hunter β, Ustanak y Lisa. Las muestras conservan OBJ/MTL, UV, normales, materiales y skinning. **Los GIF no son reproducción continua de cada fotograma**: interpolan el tiempo de visualización entre las nueve muestras.

### Ejemplos de materiales enlazados

- T-103 fase 1: 4/4 texturas originales.
- Hunter β: 2/2.
- Lisa Trevor: 9/9.
- Ustanak: 12/12.
- El Gigante: 8/8.
- Nemesis RE3 (modelo estático ragdoll): 2/2.
- Licker: mapas encontrados, pero la animación está geométricamente distorsionada.
- Wesker (`aw.mdl`): 18 de 29 materiales pudieron enlazarse a texturas disponibles; once referencias no resolvieron, por tanto su preview no es íntegro.

### Implementación y límites

- `vendors/source/animator/hl2m_native_anim89.c`: resuelve las tablas de secciones dentro del MDL y la ventana de datos adecuada para bloques externos ANI, con selección del fotograma local. Mantiene la API antigua sin buffer externo.
- `vendors/source/master/hl2m_master89.c`: función `hl2m_master_anim_external89(mdl_bytes, mdl_size, ani_bytes, ani_size, mdl, anim_index, frame, out_matrices, report)`.
- `examples/hl2m_vtf_to_tga89.c`: búfer de utilidad ampliado a 32 MiB de entrada y hasta 4096x4096 RGBA para casos como una VTF de Wesker de 11 MiB. Los límites de la biblioteca principal son independientes.
- `tests/test_anim_sections114.c`: regresión C89 de primer, segundo y último bloque.
- `tools/resident_motion_sweep114.py` y `tools/source_vtf_audit114.py`: scripts auxiliares reproducibles; requieren los archivos del usuario, que no se redistribuyen.

**No implementado en este release:** interpolación nativa de quaternions entre todos los frames, controlador de actividades/secuencias complejo, IK GPU/Source, render por fragmento completo, blending de gestures, solución comprobada de escalas anómalas del Licker, reconstrucción de malla animada para los bancos .ani sin su esqueleto de personaje fuente, y conexión Win32 en tiempo real.

### Pruebas

Compilación estricta `-std=c89 -Wall -Wextra -Werror -pedantic`; `make check`, test adicional de secciones y las exportaciones mencionadas. En los resultados brutos JSON se almacenan los códigos, nombre de motion y cantidad de huesos sospechosos por muestra.
