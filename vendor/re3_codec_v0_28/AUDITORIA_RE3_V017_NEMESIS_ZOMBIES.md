# RE3 Codec v0.17 — Némesis sin camisa y zombis

Fecha: 2026-10-31. Corpus real suministrado por el usuario. No incluye datos de Capcom en los ZIP de distribución.

## Resultados completos

Nuevo `re3emd_fidelity_audit` (ISO C89 / sin heap) analiza los clips key por key en los bancos con igual número de joints que el banco normal, comprueba las matrices y transforma al menos un vértice de cada parte de cuerpo visible por pose. Además comprueba los cuatro UV de cada quad y los tres de cada triángulo (muestreo TIM/TPAGE/CLUT).

- EMD/TIM auditados: 100/100.
- Clips compatibles enumerados: 3008.
- Clips incompatibles: 863 (se omiten de skinning, no se consideran corruptos).
- Poses decodificadas: 119536.
- Claves sin pose: 0 (dentro de bancos compatibles).
- Transformaciones de vértice muestreadas: 1375374.
- Esquinas UV probadas: 141413.
- Errores de lectura TIM: 0.

## Hallazgos específicos

| Identificación | Parts | Joints normal | Clips compatibles | Clips en bancos incompatibles | Morphs | Advertencia |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| EM34 (Némesis abrigo) | 18 | 16 | 33 | 7 | 1 | daño 15 vs normal 16 |
| EM35 (variante abrigo) | 18 | 16 | 35 | 7 | 1 | daño 15 vs normal 16 |
| EM36 (Némesis torso visible) | 24 | 16 | 37 | 8 | 8 | 14 partes no activadas por defecto; 2 morphs afectan partes principales; daño 15 vs 16 |
| EM3A (variante Némesis torso visible) | 24 | 16 | 6 | 0 | 8 | 14 partes no activadas por defecto; 2 morphs afectan partes principales |
| EM38 (Némesis criatura) | 31 | 22 | 30 | 2 | 5 | daño 15 vs 22; geometría opcional |
| EM10–EM15 | 15 | 15 | 55 c/u | 0 | 0 | normal y bancos secundarios compatibles en número de joints |
| EM16, EM18–EM1F | 11 | 11 | 38 c/u | 17 c/u | 0 | daño 15 vs 11: no reusar índices directamente |
| EM17 | 16 | 15 | 55 | 0 | 0 | una parte adicional oculta por defecto |

Advertencias: contar las articulaciones iguales NO demuestra correspondencia semántica entre dos bancos. Esto es una prueba de legibilidad y consistencia, no una demostración del comportamiento exacto del engine original. El visor emplea proyección simplificada/UV afines; no aplica morphs dependientes del estado SCD, ni implementa transición de estados de geometría opcional.

## Revisión visual

- EM36 y EM3A: anatomía del torso, cabeza y brazos ensamblada en pose base, secuencias normales visibles. En EM36 se observaron nueve fotogramas muestreados de clip 1 y seis de clip 12.
- EM10 y EM13: movimientos de extremidades/cabeza coherentes en las muestras de clip 1, sin piezas separadas obvias.
- EM18 y EM1C: revisados en varias keys; EM1C parece un personaje civil con ropa rosa, no damos por comprobada su clasificación narrativa.
- EM10 banco B, clip 1: pose pasa a caída/posición tendida; el entorno de colisiones y root motion no están reconstruidos.
- EM36 al habilitar `all` presenta piezas superpuestas y una tira geométrica: no es válido dibujar todas las partes opcionales al mismo tiempo.
- Los extremos de pies tienen aspecto triangular en varias vistas, que puede ser geometría original de bajo poligonaje o un problema del render de diagnóstico: falta contraste con ejecución original.

## Cambios v0.17

1. Añadido `tools/re3emd_fidelity_audit.c` con auditoría exhaustiva key por key + UV.
2. Añadido target Makefile y soporte MinGW para el auditor.
3. Arreglado `build_mingw.bat`: ejecución de test_animation se trasladó a después de su compilación.
4. Actualizado número de versión a 0.17.0, sin cambiar decodificación ni skinning de v0.16.
5. Imágenes comparativas y animaciones exportadas como evidencia separada.

## Pendientes reales

1. Confirmar selección de partes dinámicas en las rutinas SCD y la semántica de morphs en Némesis.
2. Mapear articulaciones para los bancos de daño de 15 joints a esqueletos de 11/16/22 joints, sin inventar rig.
3. Verificar diferencias entre los dos ejecutables PC con decompilación/depuración real.
4. Comparar timing, root motion, cuadros y calidad de textura contra capturas o trazas del juego.
5. Probar cada parte, orientación y frame, más allá de los muestreos visuales.
