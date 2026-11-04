# Fase 06 — fractional morphs + multi-group / weighted regression

## Implementado y compilado

- `src/anim/gs3d_morph_sample.c`: interpolacion de dos fotogramas por vertice
  en Q16.16, sin heap. Copia temporal estatica y restauracion por lectura
  de cada fotograma, para impedir acumulacion de deformaciones.
- `src/vendors/gs3d_vendor_morph_router.c` y `src/vendors/a{4,5,6,7,8}/*_morph.c`:
  cinco entradas por generacion, compatibilidad basada en firma MDL.
- `gs3d_sample_morph_q16` y `gs3d_vendor_sample_morph`: API publica.
- `tests/test_phase06.c`: prueba legacy (dos fotogramas 1->9), MDL7 sin
  esqueleto, rechazo de interpolacion de matrices de huesos, dos grupos con
  influencias 50/50 y modelo de ocho grupos con vertices stride 26.
- `tests/test_phase06_robust.c`: truncamientos por límites de grupo y
  rechazos de longitud de grupo y deformer.group_index incorrectos.
- `src/tools/gs3d_audit_main.c`: enumera UV, skins, stride, maxweights y
  los deformadores declarados frente a los analizados.
- Los cuatro fixtures adicionales son datos sinteticos originales, sin
  contenido de los juegos ni de Assimp.

## Investigacion documental del corpus publico

La muestra `PhosphoricAcid_MDl7.mdl` fue leida en GitHub mediante la API de
archivos, inspeccionando su cabecera y el tamano de cada grupo. No se
transfirio el archivo binario a este entorno de compilacion, por lo que NO
se ejecuto el parser local sobre el archivo real de terceros.

- Firma MDL7, 28,944 bytes, `data_size=28,944`, 8 grupos, 0 huesos.
- Cada grupo: 62 vertices, 120 triangulos, 0 fotogramas, 0 deformers.
- Primer grupo: 3 skins, 3,864 bytes. Grupos 1–7: 0 skins, 3,576 bytes.
- Registros de vertices de 26 bytes; 496 vertices y 960 triangulos en total.
- Un fixture sintetico de igual organizacion cuantitativa y longitud
  (28,944 bytes) pasa el lector local y el auditor. No sustituye una prueba
  sobre los bytes reales.

Rutas publicas:
- https://github.com/assimp/assimp/tree/master/test/models/MDL/MDL7%20(3DGS%20A7)
- https://github.com/assimp/assimp/tree/master/test/models/MDL/MDL5%20(3DGS%20A5)
- https://opserver.de/ubb7/ubbthreads.php?Number=467876&ubb=showflat

## QA

- GCC y Clang, flags `-std=c89 -pedantic -Wall -Wextra -Werror`.
- AddressSanitizer + UBSan (Clang) sobre pruebas nuevas.
- 6,231 casos de robustez heredados y 495 casos adicionales de estructuras
  multigrupo (total: 6,726 casos ejecutados, sin confundirlos con muestras reales).
- Compilaciones aisladas Legacy y MDL7 siguen funcionando.
- El compilador MinGW32 no esta instalado aqui. El script Windows compila
  las nuevas pruebas, pero no ha sido comprobado sobre Win32 real.

## Limites explicitos

- La nueva API interpola solo posiciones de vertices; las normales usan el
  extremo mas proximo. Para matrices/esqueleto de MDL7 devuelve
  `GS3D_ERR_UNSUPPORTED`, nunca simula una pose ponderada incorrecta.
- El deformer layout MDL7 en archivos autenticos con huesos/pesos sigue
  pendiente de corpus real. No validar como 'fully compatible'.
- Sin malloc/calloc/realloc/free ni operaciones float/double en C89.
- No se ha integrado el codigo en el Model Runtime principal.
