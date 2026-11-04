# Modelo Runtime Experimental — primera integración nativa de vendors GACKT

**Base:** ModelRuntime89 v19, 14 proveedores nativos (código original preservado).  
**Origen nuevo:** GACKT SDK Round22 `vendor/3dcodecs`.  
**Norma de precedencia:** las 12 implementaciones de GACKT ya representadas en ModelRuntime89 v19 no se copiaron. Las bibliotecas MMD y `recodec_c89` ya existentes en el runtime tampoco se sobrescribieron.  
**API:** ISO C89, Q16.16, sin `malloc/free` en los wrappers añadidos, almacenamiento del llamante.

## Resultado efectivo

- 14 proveedores originales conservados con sus IDs y orden de prioridad.
- 65 directorios de fuentes GACKT residuales incluidos en `vendor/gackt_3dcodecs/` (**927** archivos de fuentes, cabeceras, licencias y Makefiles).
- 6 proveedores adicionales **registrados y enlazados**: OFF (#40), OBJ (#41), PLY (#42), Irrlicht (#43), STL (#44), MD2 (#45). En total: **20 proveedores activos**.
- 59 carpetas vendorizadas **solo como fuente**, sin wrapper nativo aún. No son importadores registrados; no se compilan automáticamente. Los ejemplos y datos de prueba de GACKT se omitieron.
- `MODELRT_MAX_PROVIDERS` ampliado de 16 a 96; IDs originales 1–14 intactos.
- Acceso opcional a estructuras nativas en `include/modelrt_native_gackt89.h`, sin `.gmesh/.gskel/.gscene` ni el enrutador de GACKT.

## Compilación

```sh
make -j4 libmodel_runtime89.a
cc -std=c89 -pedantic-errors -O2 -Iinclude tests/experimental_corpus_check.c libmodel_runtime89.a -o corpus_check
./corpus_check /ruta/a/modelo.off /ruta/a/modelo.md2
```

El ejecutable de ejemplo solo lee archivos y cuenta triángulos. No se incluye corpus ni binarios en el ZIP. Compilarlo tú mismo permite repetir las pruebas en tu plataforma.

## Pruebas realizadas en esta sesión

- Compilación de biblioteca final: **EXIT 0**, GCC `-std=c89 -pedantic-errors -Wall -Wextra -O2`.
- `experimental_corpus_check`: **registered=20**.
- Corpus seleccionado Assimp: **70 ejecuciones**, 64 abrieron y recorrieron la API sin error; 6 rechazos. Resultados detallados en `EXPERIMENTAL_CORPUS_MATRIX.csv`.
- Geometría visible: `Wuson.off` 3732 triángulos; `cellar.irrmesh` 272; `cube_usemtl.obj` 12; `Spider_binary.stl` 1368; `faerie.md2` 654.
- MD2 animación: `faerie.md2`, 198 frames enumerados, primer y último visitados con éxito (654 triángulos por visita). No se infirió un FPS inexistente.
- Originales: `box.fbx` (12 tris), `anim_test.x` (840), `twospheres.ms3d` (240) y DTS sintético abrieron y visitaron por sus IDs originales.

## Limitaciones reales

- Dos PLY del corpus (`issue623.ply`, `Wuson.ply`) fueron rechazados; hay que mejorar el parser o la compatibilidad de propiedades.
- OBJ conserva líneas, puntos y metadatos en `modelrt_native_obj89()`, pero solo los polígonos generan triángulos para los visitors. Texturas de MTL referenciadas no se resuelven ni decodifican aquí.
- `.irr`: se parsean escenas, nodos y atributos como estado nativo; NO se cargan automáticamente `.irrmesh` referenciadas por la escena ni se presentan triángulos ficticios. `.irrmesh` sí emite triángulos y UV.
- PLY no expone todavía listas UV por cara y propiedades extraordinarias en la interfaz común.
- MD2 visita fotogramas discretos; interpolation/FPS y texturas externas requieren gestión del consumidor.
- Algunos códecs vendorizados de GACKT no son ISO C89 estrictos bajo todas las opciones (por ejemplo IQM utiliza `long long`); NO se registraron para no romper el build.
- No se promete que los 59 restantes funcionen a través de `modelrt_open_file()` aún; el paso siguiente es wrapper independiente por familia más corpus y regresión.
- El test original `v19-regression` exige literalmente 14 proveedores; en un build con 20 reporta `registry fail` aunque el registro esté bien. Tampoco incluye la fuente original fixtures `tiny.ms3d`. Se mantiene el test original sin alterar y se aporta `tests/experimental_corpus_check.c` para la nueva configuración.

## Archivos clave

```text
include/model_runtime89.h             ID nuevos y capacidad 96
include/modelrt_native_gackt89.h     acceso directo a structs nativos
src/model_runtime89.c                14 originales + 6 nuevos, prioridad
src/providers/provider_gackt_*.c    wrappers nativos directos
vendor/gackt_3dcodecs/*             65 carpetas deduplicadas solo códec
GACKT_VENDOR_MANIFEST.csv            SHA256 de fuentes originales
EXPERIMENTAL_PROVIDER_STATUS.csv     65 estados explícitos
EXPERIMENTAL_CORPUS_MATRIX.csv       pruebas externas
```

**Nota de alcance:** este paquete es una integración incremental verdadera, no una declaración ficticia de que los 65 vendors ya están operativos. La distribución preserva los códecs originales, incorpora todas las fuentes GACKT que no duplican los del runtime y activa seis nuevas familias que superaron las pruebas directas.
