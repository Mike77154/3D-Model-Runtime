# RE4 Classic Codec v4.9 — Corrección binaria de SEQ y recuperación de tiempos

**Corpus:** archivos `xfile.dat` y `em.dat` de RE4 PC 2007 proporcionados por el usuario. Los originales no forman parte del paquete fuente.

## Error raíz corregido

El lector anterior (`animation/re4_seq_timeline.c`) interpretaba el bloque de cuatro bytes como `{frame_q6:u16, se:u8, flags:u8}`. El barrido empírico de **12,289** SEQ reales confirma el orden **`{event_code:u8, auxiliary:u8, frame_q6:u16 little-endian}`**. El frame q6 divide entre 64, y su resto representa una fracción de fotograma. Con el lector viejo, 11,524/12,289 secuencias aparentaban tiempos no monótonos; con el lector nuevo, 12,289/12,289 quedan ordenadas. La función `re4_seq_timeline_inspect` rechaza tiempos desordenados.

Se incorpora `re4_seq_timeline_window`, que devuelve un rango de índices `[inicio,fin)` sin duplicar eventos en los límites. El código mantiene la estructura pública de `re4_seq_key` para compatibilidad con versiones anteriores, pero actualiza la interpretación de los campos.

**No atribuir semánticas sin investigación:** `se` es un código original de evento (posibles sonidos/acciones), no una orden de transformación ni un estado demostrado. `flags` es un valor auxiliar. Reconocer una marca temporal **no ejecuta** su evento nativo.

## Auditoría íntegra de DAT anidados

`re4_seq_dat_audit` lista SEQ con número de claves, tiempo máximo, eventos no nulos, campos auxiliares y, **solo cuando el FCV está inmediatamente antes en el DAT**, compara su duración. Si no está inmediatamente antes se informa `UNPAIRED`; si sus tiempos difieren se informa `TIME_MISMATCH_NOT_PROOF`. Ninguna de estas etiquetas demuestra corrupción o identifica el FCV canónico sin análisis adicional.

`examples/re4_v49_seq_corpus.py` orquesta el ejecutable C89 contra los DAT de `em.dat`, sin enlazar archivos de juego en el ZIP distribuido. Resultados:

- 176 paquetes DAT encontrados en `em.dat` (incluye familias `emXX` y `plXX`).
- 12,289 SEQ válidos, 0 malformados; **841,708 claves** leídas.
- 8,718 secuencias con duración compatible con el FCV inmediatamente anterior.
- 3,440 sin FCV inmediatamente anterior.
- 131 con duración distinta a la del FCV inmediatamente anterior y pendientes de revisión.

Estas cifras no sustituyen pruebas de SEQ nativos de audio, efectos, cambio de malla, IK o eventos de daño.

## Méndez, segunda fase (`em35`)

- El FCV `19` y el SEQ `20` son contiguos en el DAT y compatibles en duración.
- SEQ `20`: 149 claves, 41 eventos con código distinto de cero y 6 campos auxiliares no nulos; última marca en frame 148.
- **No soldar automáticamente** las mallas `em3501a`/`em3501b`. La segunda fase del jefe separa físicamente su cuerpo de las piernas; una geometría separada puede ser intencional y no constituye por sí sola un defecto. Aún no se ha demostrado el enlace entre los códigos SEQ y la visualización específica de las piezas.

## Reproducción

```sh
make clean && make -j4 all && make check
./re4_seq_dat_audit em35.dat > em35_seq_summary.csv
./re4_seq_dat_audit em35.dat --events > em35_seq_summary.csv 2> em35_event_codes.log
python3 examples/re4_v49_seq_corpus.py --em /ruta/em.dat --out resultados
```

- **C89 estricto**, memoria fija, sin `malloc`, `free` ni coma flotante en la librería y ejecutable.
- 30 suites C89, tanto con flags normales como ASan/UBSan, ejecutadas correctamente en Linux.
- **No** se han reconstruido aún la semántica nativa de cada evento, los estados de daño, las Plagas A/B/C, la vinculación visual canónica de Méndez o la integración Win32.
