# RE3 Codec v0.19 — estados explícitos y frontera de scripts SCD

**Fecha:** 31 de octubre de 2026. **No es un decompilado REA/Ghidra.**

## Hallazgo verificable del corpus del usuario

`EMD/EM36.EMD` (Némesis sin camisa): 24 partes, 8 pistas morph. Las dos pistas con partes principales son `part=1` (2 frames) y `part=2` (3 frames); las seis pistas adicionales `part=16..21` tienen 8 frames cada una y aparecen como geometría opcional en el clasificador. Las partes 22 y 23 también se consideran geometría extra. La coexistencia de geometría y morphs por sí misma NO revela qué script o evento activa cada estado.

## Estructura de scripts conocida

Las tablas del formato SCD RE3 describen `PARTS_SET=0x2A` (6 bytes), `PARTS_BOMB=0x39` (16 bytes) y `PARTS_DOWN=0x3A` (16 bytes). Referencia: https://github-wiki-see.page/m/pmandin/reevengi-tools/wiki/.RDT-%28Resident-Evil-3%29 . La biblioteca ya tenía un decodificador *estructural* para los seis bytes de PARTS_SET: opcode, align, id, type, value. v0.19 añade `re3scd_parts_probe`, que verifica ese formato solamente en un **offset elegido** del SCD extraído. Todavía no atribuye un significado ni una ruta de activación a sus operandos. No existían archivos RDT/SCD en el corpus aportado en esta conversación, por lo que el evento exacto del Némesis no ha podido confirmarse. El EXE y el RE3_MERCE fueron examinados estáticamente en fases anteriores, pero REA/Ghidra no están ejecutándose en este entorno; no afirmar equivalencia del dispatcher.

## Implementación v0.19

- `re3c_visual_state` mantiene por modelo, sin heap, 256 visibilidades y 256 índices morph.
- `re3c_visual_state_init`: estado por defecto `main` o `all` para previsualización.
- `re3c_visual_state_set_visibility`: activa u oculta una pieza con validación de límites.
- `re3c_visual_state_set_morph`: asigna un morph por pieza, sólo cuando existe la pista y el frame está en rango. Usa `RE3C_MORPH_REST` para volver a la geometría base.
- `re3c_animated_body_vertex_state`: evalúa un vértice mediante la capa anterior y el skinning de v0.18, sin modificar la geometría original.
- `re3emd_anim_preview`: parámetro opcional 14, archivo de eventos ordenados por key para **control manual diagnóstico**. Incluye las keys exactas de evento en las capturas incluso si no caen en el muestreo periódico; cámara fija dentro de la secuencia. Ejemplo en `examples/EM36_explicit_state_demo.txt`.
- `re3scd_parts_probe`: interpreta una sola instrucción `PARTS_SET` en un offset indicado de un `.SCD` extraído. No ejecuta scripts ni barre arbitrariamente bytes.

Formato del archivo de eventos:

```
# key part operation [argument]
0 16 hide
10 1 morph 1
15 16 show
15 16 morph 2
30 1 morph rest
36 16 hide
```

Ejemplo real con EMD/TIM proporcionados por el usuario (archivos **no** incluidos en el paquete):

```sh
make
mkdir -p anim_estado
./re3emd_anim_preview EM36.EMD EM36.TIM anim_estado 0 1 16 front texture model main rest no-map examples/EM36_explicit_state_demo.txt
```

El archivo de ejemplo demuestra el mecanismo, **no reproduce eventos originales confirmados de Capcom**. Para identificar los activadores auténticos se necesitan al menos scripts SCD extraídos de RDT, las referencias de enemigo y estados, y/o una traza dinámica del ejecutable original.

## Pruebas realizadas

- GCC C89 `-std=c89 -pedantic -Wall -Wextra -Werror -O2`: `make`, `make test` PASS.
- Clang C89: compilación de núcleo, visor de estados y prueba sintética con EM36 PASS.
- UBSan de GCC: prueba de estado con EM36 PASS, sin errores observados.
- `test_visual_state` sintético y con `EM36` real: PASS; incluye estado independiente por parte, morph/rest, rechazos por límites y atomicidad de setters.
- `test_parts_set`: decodificación de comando SCD sintético y rechazos de longitud/opcode: PASS.
- `re3emd_motion_audit`: 100/100 EMD del corpus original, sin errores.
- Secuencia de Némesis con 10 eventos externos: render de 16 imágenes, GIF de comparación generado con Pillow (no es dependencia del códec).
- Archivo de eventos con morph inexistente: rechazado sin renderizar.

## Pendientes relevantes

1. Extraer e interpretar scripts de los RDT de las escenas de Némesis, vincular identificadores de enemigo y eventos a comandos SCD.
2. Reconocer si morph frame se selecciona realmente por script, por estado interno de enemigo, interpolación automática o mezcla de varios controles.
3. Descubrir qué piezas opcionales son alternas/sustitutos, tentáculos u otros estados, y qué geometrías son simultáneas.
4. Confirmar semántica del dispatcher SCD de `ResidentEvil3.exe` y `RE3_MERCE.exe` mediante desensamblado más profundo, y reproducirlo sobre datos concretos.

**Límite de la fase:** v0.19 ofrece instrumentación verificable para reconstituir esas transiciones, no una reimplementación certificada del comportamiento original de Capcom.
