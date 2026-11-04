# HOD2 Phase11 — deformacion real por matrices MOT (C89)

## Entregado

`hod2_skin_glb.c` es un modulo **nuevo ANSI C89**, sin `malloc`, `calloc`, `realloc`, `free`, `float`, `double`, `stdin`, ni dependencias de terceros. Este modulo interpreta los descriptores reales de piezas de los EXE y sus matrices obtenidas de los frames MOT. Mantiene POS/NORMAL de fuentes EXE/POL e UV del POL. Exporta GLB 2.0 con `skins`, `JOINTS_0`, `WEIGHTS_0` y `inverseBindMatrices`, con animacion rotacional y traslacion raiz. Las matrices se calculan con punto fijo Q14 (rotacion) / Q12 (posicion), sin funciones de punto flotante.

**Transformacion equivalente (convencion de matrices columna):** posicion original del grupo `p_bind = J_group(frame0) * p_source`. Para un vertice sin override se toma `J_draw_bone(frame0)*p_POL`. En el GLB el joint especifico tiene `IBM = inverse(J_group(frame0))`, de modo que la pose animada es `J_group(frame) * IBM * p_bind`. No se deducen pesos por proximidad: cada referencia recibe el unico grupo de matriz proporcionado por el EXE. Si dos grupos escriben el mismo vertice, el ultimo se impone (registros 0-3).

La raiz recibe su traslacion MOT; los otros huesos conservan desplazamientos relativos del EXE. Se aplica el orden de euler del juego convertido a cuaterniones glTF, con normalizacion y continuidad de signo. El CORDIC cubre tambien angulos mayores a 90 grados mediante reduccion de cuadrante. Cada TEX original se identifica por par (bank,slot), se convierte a PNG compatible y se embebe en el GLB; los formatos paleteados que carecen de paleta siguen sin textura.

## Comandos

```
hod2c89.exe motdump 1999 "Hod2 - y99.exe" "mot" 15 187 "tipo15_mot187.h2mc"
hod2c89.exe skinpart 1999 "Hod2 - y99.exe" 15 0 "pol" "tipo15_mot187.h2mc" "abdomen15.glb" "tex"
hod2c89.exe skincharacter 1999 "Hod2 - y99.exe" 15 "pol" "tipo15_mot187.h2mc" "personaje15.glb" "tex"
```

`skinpart` exporta la pieza deformable por separado. `skincharacter` exporta **un personaje completo**, integrando modelos rigidos (como pesos de hueso 1.0) y piezas deformables de los descriptores soportados del EXE en la misma malla/glTF skin y bajo el mismo MOT. No incluye las piezas de la ruta especial del tipo 23; el campo `unresolvedParts` de extras informa cuantas referencias no se incluyeron. El comando previo `character` no cambia: sigue usando su ruta rigida de Phase08.

Los directorios POL/MOT/TEX y EXE son recursos que debe proporcionar el usuario, y no se incluyen en esta entrega por separado. `build_win32.bat` compila el nuevo modulo para MSYS2 MinGW32 con GCC C89 estricto, pero no se ha probado compilacion sobre Windows real en esta sesion.

## Pruebas observadas

- `make check`: pruebas previas aprobadas (GCC -std=c89 -pedantic -Wall -Wextra -Werror).
- Compilador Clang C89 estricto: compila sin errores ni warnings.
- AddressSanitizer/UndefinedBehaviorSanitizer Clang: exportacion real de tipo 4, tipo 15 y tipo 76 sin errores detectados.
- `validate_phase11.py` (herramienta externa de QA, **no parte del codigo del runtime**) valida estructura GLB 2.0, offsets de accessors, imagenes PNG, posicion inicial de la malla contra IBM/huesos, finitud de atributos, pesos y cambio de posiciones en frame 10, `trimesh`. Los 11 GLB auditados aprobaron; ver `phase11_kinematic_validation.csv`.
- Tipo15, motion187: 15 partes rigidas, 1 deformable, 0 referencias sin resolver; 16 huesos, 28 primitivas, 699 triangulos, 8 texturas, 31 frames. Ediciones 1999 y 2000: GLB resultante igual byte por byte con los bancos provistos. Existe desplazamiento de vertices real, no solo animacion de huesos.
- Tipo4: 15 partes rigidas + 2 deformables, 0 referencias sin resolver, 27 imagenes.
- Los tipos 1, 9, 23, 31 y 76 aun presentan referencias no resueltas en la exportacion completa; el CSV las contabiliza, en vez de afirmar falsamente exportacion total.

## Limitaciones

1. `skincharacter` no reproduce todavia el algoritmo exclusivo de cola articulada del tipo 23 ni los formatos POL alternativos o las piezas faltantes de algunos tipos.
2. No se han validado visualmente en un renderizador de juego todos los 86 tipos ni todos los clips MOT. Una transformacion estructural valida no demuestra que todas las costuras, normales o accesorios se vean exactamente como en el ejecutable.
3. Se usa influencia unica para la asignacion efectiva por vertice (la ruta de escritura del motor reconstruida). No se generan pesos lineales sinteticos ni interpolacion multipeso.
4. Calculos fijos Q14/Q12 cuantizan la pose; error de IBM inicial observado en los GLB auditados <=0.027 unidades de matriz, error al reconstruir una posicion de bind <=0.01 unidades. Es una aproximacion, no equivalencia bit por bit al x87 de 24 bits del original.
5. La ruta nueva se ofrece como `skincharacter`/`skinpart`, sin sustituir automaticamente todos los consumidores de `charactertex` ni el runtime externo.

## Fuentes de formato

`hotd2-re-main` aportado por el usuario: `src/game/skin.cpp`, `skin.h`, `anim.cpp`, `anim.h` para conocer la semantica original de las tablas EXE/MOT/POL. Esta entrega contiene una implementacion nueva e independiente en C89. Mantener avisos de licencia del proyecto de referencia al redistribuir cualquier fuente del mismo.
