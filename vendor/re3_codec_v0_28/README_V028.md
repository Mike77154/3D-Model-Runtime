# RE3 Codec v0.28 — seguimiento de la ruta de daño (Ghidra + objdump)

## Hallazgo directamente sustentado

`ResidentEvil3.exe` y `RE3_MERCE.exe` tienen rutinas de muestreo de EMR
que escriben canales de rotación en nodos **contiguos**. Los offsets son:

| Operación | RE3 PC | RE3 Mercenarios |
|---|---|---|
| Key sampler, flags | 0x004367F0 | 0x00436840 |
| Pose transition preparation | 0x00431F70 | 0x00431FC0 |
| Key decode into node slots | 0x004368B0 | 0x00436900 |
| Interpolation into node slots | 0x00436ED0 | 0x00436F20 |
| Angular correction between slots | 0x004322A0 | 0x004322F0 |

El contador `param_1[1] & 0xffff` en el decodificador limita el número de
nodos actualizados. Su destino empieza en `object->field_108` y las
escrituras de rotación se realizan con incremento de `0xBC` bytes por nodo.
La interpolación en `0x436ED0` usa el mismo patrón de `+0xBC` por nodo.

Esto **favorece** una interpretación en la que un banco de daño de 15
canales sobre un modelo de 16/17 nodos actualiza únicamente los primeros
15 nodos y deja los nodos restantes con su transformación anterior.
**No demuestra** qué pose tiene el juego al entrar en daño, ni elimina
los estados de partes y transformaciones especiales del enemigo.

Las direcciones globales `0x00A5E78C` y `0x00A5E790` reciben punteros
EDD/EMR desde varias funciones; el objdump empleado muestra 26 escrituras
en cada una y ninguna lectura directa. Ese dato no excluye accesos
indirectos o procedimientos fuera de este ejecutable. 

## Implementación experimental en C89

`re3c_damage_overlay_prefix(model, base, damage, dst)` copia una pose
`base` de articulaciones del banco A y sobrescribe sólo los canales de
rotación presentes en el banco de daño. El llamador debe aportar la pose
previa/reference real para los nodos restantes. Se rechazó automáticamente
cualquier banco con más canales de daño que nodos de referencia.

El visor ofrece `damage-prefix` **opt-in**. Para las capturas de demostración,
`base` es la primera pose del primer clip normal. El juego puede usar otra
pose anterior y otra transformación de raíz: por eso sigue siendo experimental.

## Pruebas

- Compilación GCC y Clang C89 estricto, pruebas sintéticas y UBSan.
- 100 archivos EMD: 35 compatibles estructuralmente con el experimento,
  24 rechazados por exceso de canales respecto al esqueleto principal,
  41 sin banco de daño.
- `EM30`, `EM33`: 40 keys de daño por variante; preservan 2 nodos sin sobrescribir.
- `EM34`, `EM35`: 446 keys por variante; preservan 1 nodo sin sobrescribir.
- `EM36`: 324 keys; preserva 1 nodo sin sobrescribir.
- En variantes `EMD08`, también se comprobó el nuevo modo donde estaba disponible.

**Resultado visual:** en Némesis hay diferencias notables respecto al mapa
anterior; en Grave Digger el cuerpo sigue demasiado comprimido. NO está
resuelta la fidelidad de sus 40 keys ni el comportamiento especial de daño.
El éxito significa decodificación y transformación con pruebas de seguridad,
no equivalencia demostrada con Capcom.

## Reproducir

```bat
build_mingw.bat
re3emd_damage_prefix_audit.exe EM30.EMD
mkdir grave_prefix
re3emd_anim_preview.exe EM30.EMD EM30.TIM grave_prefix 2 0 12 quarter texture model main rest damage-prefix
```

A diferencia del código anterior, ya no se inserta un canal neutro
arbitrario en Némesis. Las funciones viejas siguen disponibles como
comparativas explícitas.

Siguiente investigación: determinar qué pose alimenta los nodos no
actualizados, cómo se manejan estados de partes y morphs, y comparar
con una traza en ejecución del juego cuando una criatura recibe daño.
