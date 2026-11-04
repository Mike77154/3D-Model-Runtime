# HOTD2 Phase09 — Auditoria reproducible sobre bancos BIN reales

Entrada: ZIP proporcionados por el usuario, `hod2ye99.zip` y `hotd2y2000.zip`.
Los CSV en `reports/` se generaron sobre la extraccion de los dos ZIP. No se
adjuntan los bancos comerciales en el paquete de fuentes.

## Versiones y cobertura

| Metrica | y99 | y2000 |
|---|---:|---:|
| Todos los archivos BIN | 1241 | 1187 |
| POL | 656 | 656 |
| TEX | 492 | 438 |
| MOT | 50 | 50 |
| POL validados | 650 | 650 |
| MOT con directorio valido | 49 | 49 |
| Modelos POL contabilizados (incluye alias) | 18027 | 18027 |
| Triangulos POL contabilizados (incluye alias) | 2622122 | 2622122 |
| UV IEEE no finitas en POL aceptados | 0 | 0 |
| Bloques MOT apuntados por directorio | 1107 | 1107 |

Comparacion bit a bit de los 1187 BIN compartidos: **1182 identicos,
5 diferentes** (3 POL, 2 TEX), con otros **54 TEX solo en y99**.
Se identificaron 316 contenidos POL unicos por edicion entre los 656 nombres
(digest SHA-256 del archivo sin decodificar); el resto son alias o copias.\nEs decir, los 50 bancos MOT son identicos entre ediciones. No significa
que el movimiento generado o los tipos de personaje coincidan con fidelidad.

### Archivos con validacion de formato pendiente (ambas versiones)

- POL: `komono_0.bin`, `tv2.bin`, `bg_adv19.bin`: ficheros de 987 bytes,
  sin la cabecera POL compatible con el decodificador. No se clasifica el
  motivo de la diferencia con este test.
- POL: `pol_boss2_hane_hod1.bin`, `pol_znonoo.bin`,
  `pol_komono_suimonie.bin`: no pasan el lector de tablas POL habitual;
  pueden ser otra representacion o ficheros no conformes. Sin evidencia
  para implementar una interpretacion por conjetura.
- MOT: `tt.bin`: longitud 88 bytes; su primera entrada indica un directorio
  de 88 bytes y las siguientes referencian desplazamientos fuera del
  archivo. No se puede tratar como banco de clips completo.

### Distintos entre ediciones

- POL: `bg_adv19.bin`, `komono_0.bin`, `tv2.bin`
- TEX: `scr_blood_green.bin`, `scr_tod_itm_itamidome2.bin`

La comparacion fue verificada adicionalmente contra lectura y hash de ZIP
independiente. La compilacion del C89 nativo paso con GCC y Clang (`-std=c89
-pedantic -Wall -Wextra -Werror`); la auditoria real en Clang y su ejecucion
con sanitizadores ASan/UBSan produjeron CSV byte-identicos a los de GCC.
No hay afirmacion de compilacion cruzada Win32 en este entorno.

### Limitaciones y proxima conversion

Los ZIP aportan los bancos pol/tex/mot/cam/evt/coli pero carecen de los
ejecutables `Hod2 - y99.exe` / `Hod2 - y2000.exe`; esas tablas son necesarias
para inferir materiales TEX por indice, uniones de huesos, `CharacterDrawBonePart`,
`DrawCharacterPartSubparts` y asociacion de clips sin suposiciones.
Por eso esta fase **no** certifica skinning, abdomen, costuras ni los 86
GLB completos. Los dos GLB anexos son mallas estaticas reales con UV,
sin textura y sin animacion.

### Ejecutar

Descomprimir cada ZIP bajo carpetas `hod2ye99` y `hotd2` y usar:

```sh
hod2c89 corpus ./hod2ye99 audit99.csv
hod2c89 corpus ./hotd2 audit00.csv
hod2c89 corpusdiff ./hod2ye99 ./hotd2 diff.csv
```

Las columnas `models` y `triangles` de los CSV son la suma por archivo;
la suma global cuenta varias veces ficheros repetidos bajo otros nombres.
