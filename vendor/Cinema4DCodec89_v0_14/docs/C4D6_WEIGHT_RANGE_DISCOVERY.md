# XC4DC4D6 — Weight: recuperación comprobada de índices de puntos

**Investigación local, 10 de octubre de 2026.** No se usa el SDK de Maxon para decodificar. El siguiente esquema está respaldado por bytes de muestras reales del corpus, **no** constituye una especificación universal del formato C4D.

## Qué sí demostramos

El texto UTF-16BE `Weight` aparece tanto en registros de datos como en la tabla de nombres. Un registro de clase nativa en las muestras examinadas usa el prefijo:

```
0F 00 0F 8D E5  0F 00 00 00 00  82 00 00 00 0C
00 57 00 65 00 69 00 67 00 68 00 74   (= Weight)
```

Al desplazamiento relativo `+86` aparece `80 00 00 00 0E 61 6C 69 61 73 32` (`alias2`). El patrón siguiente se verificó en siete tags con entradas:

| Offset relativo al comienzo de la clase | Estructura observada | Interpretación admitida |
|---|---|---|
| `+169` | `0F` + `BE32 N` | Número de grupos de rangos |
| `+174` | `80` + `BE32 12*N` | Número de bytes de la tabla |
| `+179` | `N × (LE32 inicio, LE32 final, LE32 prefijo)` | Rangos inclusivos de índices, comprimidos |
| `+179+12*N` | `10` + `BE32 64` | Campo de control, significado no identificado |
| `+184+12*N` | `10` + `BE32 puntos` | Número total de índices enumerados |
| `+189+12*N` | `10` + `BE32 desconocido` | Campo de control, significado no identificado |
| `+194+12*N` | `10` + `BE32 64` | Campo de control, significado no identificado |
| `+199+12*N` | `0F` + `BE32 índice_máximo` | Último índice de punto de los rangos |
| `+204+12*N` | `80` + `BE32 256` | Byte count de máscara redundante |
| `+209+12*N` | 256 bytes | Máscara de bits, bit `i%8` del byte `i/8` |

La columna «prefijo» guarda la cantidad de índices de rangos anteriores. Por ejemplo, los primeros registros de `FemaleHobo` son `(0,9,0)`, `(41,77,10)`, `(88,118,47)`, `(124,125,78)`. Son índices de puntos **consistentes con** las mallas presentes. La representación por bitmap reproduce **exactamente** el mismo conjunto, sin bits adicionales, en las siete tablas positivas probadas.

## Comprobaciones numéricas

| Muestra | Tags con rangos | Rangos | Índices recuperados | Mayor índice | Vértices de la malla candidata |
|---|---:|---:|---:|---:|---:|
| FemaleHobo | 1 | 17 | 220 | 1246 | 2025 |
| FemalePunk | 1 | 9 | 169 | 1499 | 1837 |
| MalePunk | 1 | 4 | 136 | 638 | 2264 |
| ENP1, tag 0 | 1 | 28 | 63 | 260 | 261 |
| ENP1, tag 1 | 1 | 18 | 525 | 1146 | 1147 |
| ENP1, tag 2 | 1 | 11 | 17 | 246 | 250 |
| ENP1, tag 3 | 1 | 5 | 11 | 175 | 314 |

En `appearance`, `L_walk_b` y `at_jump_knock_a` el campo N vale cero: el inventario reconoce un tipo de tag vacío, pero no asegura ausencia general de pesos o deformaciones en esas escenas.

## Contrato del motor

- `c4d89_inventory_weight_tags()` reconoce registros **de clase** `Weight`, no las referencias textuales a la palabra.
- `layout=4` significa que la tabla LE32 de rangos y el bitmap fueron verificados y son idénticos.
- `c4d89_stream_weight_point_ids()` emite `(point_index, ordinal)` y valida nuevamente el bloque antes de emitir.
- `layout=3` significa rangos y suma validados **sin confirmar la máscara**, por ejemplo en otra variante del archivo; `layout=2` sólo reconoce un blob opaco.
- Ni el ordinal ni un rango equivalen a `joint_index`, `influence_weight`, `parent_node`, `inverse_bind` o `rest_pose`.
- El orden de tags frente al orden de mallas es una correspondencia **candidata**, aun cuando las cantidades de mallas y tags coincidan.

No alimentar todavía los IDs al kernel de skinning como si fueran pesos por hueso.

## Pruebas

```
./scripts/build_posix.sh
python3 tests/test_weight_inventory.py
./build/test_skin_core
```

`test_weight_inventory.py` necesita muestras del corpus de la persona usuaria, colocadas en `$C4D89_CORPUS_DIR`. Rechaza bloques con prefijos, sumas y valores de máscara manipulados. Las máscaras incoherentes degradan su certificación de `layout 4` a `layout 3`, en vez de producir falsos pesos.

## Investigación siguiente

Descifrar los datos inmediatamente posteriores a la máscara de 256 bytes para determinar si contienen **índices de huesos, influencias escalares y/o reposo**. Correlacionarlos contra joints y efectos deformados en al menos dos modelos con el mismo número de puntos pero distintos pesos. Finalmente confirmar enlaces padre-hijo y matrices inversas de bind contra una escena evaluada en Cinema 4D.
