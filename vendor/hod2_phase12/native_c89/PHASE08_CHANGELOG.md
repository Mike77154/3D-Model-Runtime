# HOD2 C89 Phase08 — Rigid Character Assembly + MOT + TEX + Batch

## Resumen funcional

Phase08 añade un recorrido nativo que toma un ejecutable **aportado por el usuario**, carpetas POL/TEX extraídas y un clip H2MC opcional. Reconstruye el **árbol rígido** del personaje y exporta GLB 2.0 con nodos hueso, mallas por hueso, UV originales, materiales TEX disponibles y, si se solicita, una animación con canales `rotation` (todos los huesos) y `translation` (raíz).

Los 17 scripts Python y las 3 tareas Java originales permanecen dentro del paquete completo como **referencia**, no se presenta esta fase como una migración 1:1 de todos ellos.

## Nuevos comandos nativos

Ejecutar desde `native_c89` después de compilar; los directorios de salida deben existir:

```bat
hod2c89.exe character 1999 "Hod2 - y99.exe" 7 "POL" "tipo07_rigido.glb"
hod2c89.exe character 1999 "Hod2 - y99.exe" 7 "POL" "tipo07_animado.glb" "tipo07.h2mc"
hod2c89.exe charactertex 1999 "Hod2 - y99.exe" 7 "POL" "TEX" "tipo07_texturizado.glb" "tipo07.h2mc"
```

Para extraer primero el movimiento sin alterar los bits originales:

```bat
hod2c89.exe motions 1999 "Hod2 - y99.exe" "MOT" 7 "candidatos07.csv"
hod2c89.exe motdump 1999 "Hod2 - y99.exe" "MOT" 7 123 "tipo07.h2mc"
```

*El ID `123` es un ejemplo de sintaxis, no un ID confirmado para el tipo 7.* El clip H2MC debe coincidir con el perfil, tipo y número de huesos del PE.

Exportación masiva (1 versión por ejecución, máximo 86 tipos):

```bat
hod2c89.exe batch 1999 "Hod2 - y99.exe" "POL" "TEX" "SALIDA" "reporte99.csv"
hod2c89.exe batch 2000 "Hod2 - y2000.exe" "POL" "TEX" "SALIDA2000" "reporte2000.csv" "H2MC_2000"
```

La carpeta H2MC opcional usa nombres `type00.h2mc` a `type85.h2mc`. Un archivo ausente exporta pose rígida sin animación. Un archivo presente pero inválido provoca un fallo declarado en el CSV, **no** una asignación automática de un clip incorrecto.

`TEX` puede reemplazarse por `-` para no buscar imágenes. El comando `batch` termina con código distinto de cero si algún tipo no pudo exportarse; conserva siempre los resultados de los demás en el CSV. Una exportación marcada `ok` puede seguir teniendo referencias rígidas ausentes (`unresolved_rigid`) o partes deformables no exportadas.

## Detalles técnicos de la animación

- `MOT` original mantiene su extracción H2MC binaria sin pérdida.
- `H2MC` se valida por formato, versión, perfil, tipo, huesos, stride, longitud, ticks y cuadros. Límite de 2048 muestras por clip.
- `i16` angular usa escala `2*pi/65536`. La conversión Euler `xyz` extrínseca a cuaternión usa CORDIC entero Q15; **aproximación**, no el resultado binario exacto de SciPy.
- Los tiempos 30 Hz se escriben como `float32` correctamente redondeado usando **solo enteros de 32 bits**.
- Traslación de raíz y atributos POL (posiciones, normales, UV) se preservan como bits originales `float32` sin realizar cómputos de coma flotante en el programa C89.
- Las rotaciones tienen corrección de hemisferio para interpolación lineal `LINEAR` en glTF.
- `charactertex` resuelve materiales por pareja **(banco POL, índice TEX)** para evitar mezclar texturas entre bancos. PNG compatible se embebe en el BIN del GLB.
- Se mantiene formato original de los nodos y sus posiciones relativas, sin uniones geométricas inventadas.

## Memoria fija y Win32

Los workspaces de entrada PRS/POL/TEX son ahora compartidos entre la CLI, los bancos MOT y la exportación. El buffer H2MC de Phase08 se acota a 2 MiB, evitando el gasto antiguo de 64 MiB. Medición local en Linux de la versión CLI: **~237 MiB de BSS** (vs. ~491 MiB antes de compartir los buffers). Sigue siendo memoria estática, sin `malloc` ni heap. No se ha compilado todavía en un entorno real Win32/MinGW32.

## Pruebas efectivas

- GCC y Clang: `-std=c89 -pedantic -Wall -Wextra -Werror` — pasan.
- Clang Address/Undefined y GCC Address/Undefined sobre un personaje sintético de dos huesos — pasan después de corregir una aritmética de punteros en el generador de fixtures.
- GLB de dos huesos generado nativamente: 2 meshes, jerarquía 0 → 1, 4 cuadros a 30 Hz, rotaciones 0°–135° del hueso hijo.
- Verificación Python externa (solo para comparar resultados, **no necesaria para ejecutar C89**): `trimesh` carga el GLB, PNG embebido es válido y mayor error de componente de cuaternión frente a `scipy.spatial.transform.Rotation`: **1.449603e-05** para esta prueba. Tiempos `float32` coinciden bit a bit con `np.arange(4,dtype=np.float32)/30`.
- Prueba de lote con un PE sintético que define solo el tipo 0: 1 exportado, 85 fallos explícitos y 87 líneas CSV (cabecera + 86 tipos). **No** prueba los 86 tipos reales.
- Fixtures GLB incluidos en `examples/` son **sintéticos**, no contienen modelos comerciales de HOD2.

## Pendientes importantes

1. `CharacterDrawBonePart`/`DrawCharacterPartSubparts`: mapeo real por vértice para abdomen, tórax, piezas extra y deformación por grupos, y generación de `JOINTS_0`, `WEIGHTS_0`, `skins`, `inverseBindMatrices` usando las matrices apropiadas.
2. Texturas paletizadas y formatos TEX no soportados; reconocer y avisar con detalle por primitiva.
3. Clips múltiples por personaje, selección automática fundada en tablas de despacho y sincronización; H2MC es **un clip por GLB**.
4. POL de ruta alterna (flag `0x10`), otros índices y accesorios no incluidos en el árbol estándar.
5. Previsualizador/rasterización y validación geométrica real de costuras, UV y clips por los 86 × 2 activos. **Faltan EXE y BIN comerciales** en el ZIP de origen para hacer estas pruebas.
6. Reemplazo de scripts de Ghidra: la decompilación real sigue dependiendo de la API de Ghidra y no ha sido portada a C89.

No debe confundirse la exportación de jerarquía **rígida** con skinning del personaje entero. En especial no se afirma que hayan desaparecido huecos o abdominales ausentes de todos los modelos.
