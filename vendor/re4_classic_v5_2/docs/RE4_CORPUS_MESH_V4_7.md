# RE4 Classic Codec v4.7: auditoria escalonada de mallas

La v4.7 añade a `re4_mesh_sequence_audit` el modo `--summary`:

```
./re4_mesh_sequence_audit MASTER.pmd CLIP.fcv 0 30 15 PIEZA.pmd --summary
```

Salida C89, campos: `status,frames,parts,vertices_per_frame,changed_frames,max_step_q16,attention_frames,near_i32,stationary_links,linked_rotations`.

`exit code 7` es una **advertencia geométrica**, no fallo de lectura: se debe
interpretar como `MESH_ATTENTION` y revisar su columna `attention_frames`.
La política actual marca cambios de malla por encima de 250 unidades de modelo
y vértices cercanos a saturación entera. El umbral no puede demostrar errores
visuales; un cambio brusco también puede deberse a un movimiento válido.

`examples/re4_v47_mesh_corpus.py` enumera archivos desde xfile.dat/em.dat
privados, ejecuta las herramientas C89 y produce:

* `clip_samples.csv`: hasta tres poses por FCV accesible. No es full-frame.
* `multipart_candidates.csv`: subconjunto explícito de 19 ensamblajes
  candidatos. No confirma diseño canónico, armas, indumentaria ni IK.
* `whole_clips.csv`: un FCV completo por familia con master enlazado.
* `families.csv` y `SUMMARY.md`.

Opciones útiles para investigación en máquina con los recursos originales:

```
python3 examples/re4_v47_mesh_corpus.py \
  --xfile /ruta/xfile.dat --em /ruta/em.dat \
  --samples /ruta/sampled_fk_by_family.csv --out /ruta/auditoria
python3 examples/re4_v47_mesh_corpus.py \
  --xfile /ruta/xfile.dat --em /ruta/em.dat --out /ruta/aves \
  --all-frames --family em23 --family em25 --family em29 \
  --skip-multipart --full-clip-budget 0 --seconds 10000
```

`--all-frames` recorre el FCV completo pero puede requerir mucho tiempo para
miles de clips: no debe confundirse con el modo de tres muestras. `--seconds`
limita la fase de muestreo; la matriz señala si el presupuesto interrumpió
la ejecución. `--override em31=em3101.pmd` selecciona un PMD **explícito**,
sin implicar que sea el cuerpo canónico o el conjunto completo.

`examples/re4_v47_complete_index.py` recorre además los DAT principales,
los archivos *low* y los recursos de armas, enlazando su estado con los
resultados de geometría. Inventariar ≠ decodificar y animar un personaje.

## Resultados reales de la investigación de 2026-11-02

Usando los recursos originales del usuario, el primer barrido intentó
5,173 FCV no vacíos de 62 familias con PMD enlazable seleccionado:
4,723 muestras sin fallo, 449 sin canales de rotación enlazados y una
advertencia geométrica. Cambiar explícitamente el PMD de `em31` a `em3101`
y `em3c` a `em3c00a` recuperó 138 muestras adicionales (83 y 55).
La cifra combinada es 4,861 FCV con mallas procesadas, 311 sin rotaciones
compatibles y 1 alerta. Es una prueba de POSES MUESTREADAS, no del clip entero.

* 56 FCV completos, uno por familia elegible en el primer barrido, pasaron.
* Otros dos FCV completos de `em31` y `em3c` pasaron con masters alternativos.
* Los 147 FCV de `em22`, `em23`, `em24`, `em25`, `em28` y `em29`
  recorrieron **todos sus fotogramas** con éxito de FK+skinning para su PMD base.
* 17 de 19 montajes candidatos pasaron; `mendez_phase_b` no enlazó su pieza
  complementaria y `armadura_a` no enlazó por el esqueleto incompatible.
* `em38` FCV 42 activó 1 alerta en tres muestras por un desplazamiento de
  18,846,138 Q16 (~287.6 unidades). No significa corrupción confirmada.

## No cubierto todavía

Parásitos cabeza A/B/C sin PMD definitivo, armadura A y Mendez fase 2
multipartes, IK/root native y transiciones de estado, mallas LOD y armas
como piezas acopladas, transparencias, texturas, efectos y reproducción
canónica en Win32. Las pruebas de compatibilidad de huesos nunca sustituyen
la verificación visual del clip completo.

El fuente no contiene PMD, FCV, DAT, texturas ni modelos de Capcom.
