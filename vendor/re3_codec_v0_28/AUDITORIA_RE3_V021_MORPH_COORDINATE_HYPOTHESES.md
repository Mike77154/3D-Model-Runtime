# RE3 codec v0.21 — interpretación de morphs y diagnóstico de Grave Digger

## Hipótesis y evidencia

En EM30 y EM33 los morphs posteriores al frame 0 de las partes 1, 2, 3 y 8 muestran centros casi en cero, mientras sus vértices MD2 están separados varios miles de unidades en X. Interpretarlos como posiciones locales absolutas separa las piezas.

| Parte | Centro X base | Centro X morph 1 |
| --- | ---: | ---: |
| 1 | -829 | -4 |
| 2 | -2121 | 0 |
| 3 | -4673 | -2 |
| 8 | 2256 | 188 |

Al sumar las coordenadas de morph 1 a cada vértice local original, la continuidad del modelo durante el clip 4 mejora visiblemente. La misma técnica evita la desaparición espuria de parte de la cabeza en EM36. Esto **apoya** una hipótesis de DELTAS a partir del frame 1; aún no es prueba de semántica original ni garantiza el resultado para todos los juegos.

En los 100 EMD aportados se inspeccionaron 50 pistas morph, 273 estados y 223 estados posteriores al 0. De 2,227 vértices en frames 0, 1,971 coinciden exactamente con MD2; 52 de los 223 estados posteriores reúnen el criterio estricto de centro relativamente cercano al origen, que detecta desplazamientos evidentes y no todos los casos delta. Las 100 lecturas no tuvieron errores.

## Implementación nueva

- `re3c_animated_body_vertex_morph_delta`: frame REST y 0 usan lectura anterior; frames posteriores suman el morph local al vértice MD2 antes de skinning DAT y rotación.
- `re3c_morph_center_translation` y `re3c_animated_body_vertex_morph_offset`: alternativa diagnóstica de desplazamiento de centro.
- Visor: opción final `native|recenter|delta`, con cache por pieza/frame; sin cambio en el comportamiento por defecto.
- `re3emd_morph_space_audit`: CSV de centros por pista y estado, coincidencias del frame 0 y detección conservadora de candidatos.
- `test_morph_delta`: compara APIs en EM30 y verifica límites y estado REST.

## Condiciones y límites

Se conserva ISO C89, memoria estática y aritmética fixed point. No se incluyen recursos de juegos ni ejecutables propietarios en el paquete de código fuente. Este cambio no valida el remapeo de los bancos de daño de 15 a 17 articulaciones ni reconstruye eventos SCD.

El motor PC y REA/Ghidra siguen pendientes de estudio cruzado: **no se ha ejecutado ni descompilado el binario original para confirmar cómo evalúa los morphs**. La correlación geométrica observada es un indicio importante pero no una confirmación oficial.
