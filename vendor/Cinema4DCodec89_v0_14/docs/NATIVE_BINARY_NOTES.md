# XC4DC4D6 — Evidencia experimental de la serializacion geometrica

Esta nota documenta estructuras comprobadas por *ingenieria de reversa*
sobre 260 escenas del corpus del usuario. No es una especificacion oficial de Maxon.
Los numeros de bytes corresponden a offsets en el archivo original `.c4d`.

## Registros observados

### UV — marcador 0x85

```
85 [longitud: u32 BE] [4 * poligonos * 3 * f32 BE]
```

Con `u`, `v` por esquina y tercer componente `w=0` en las muestras.
La longitud es 48 * total_poligonos; despues aparece el footer
`15 00 02 02` (observado, no generalizable).
Los UV pueden ser NEGATIVOS O SUPERIORES A 1: tiling/proyecciones.

### Indices — marcador 0x86

```
86 [longitud: u32 BE] [control: 00 00 00 01] [caras de 4 * u16 BE]
```

Longitud = 4 + 8 * poligonos. Un triangulo tiene indices (a,b,c,c).
El footer de estos registros observados es `02 02 01 00`.
La version C89 acepta tambien (a,b,c,d) como 4 esquinas si d!=c,
aunque el corpus auditado era triangular.

### XYZ — marcador 0x85

```
85 [longitud: u32 BE] [vertices * 3 * f32 BE]
```

Footer observado `02 02 02 02`. Se valida que indice_max < vertices.
XYZ y UV convierten IEEE754 big-endian a Q16.16 con desplazamientos
enteros sin `float`, `double`, `long long` ni memoria dinamica.

### Verificacion conjunta

Las 3 tablas tienen una relacion estricta:

```
num_uvw = 4 * num_poligonos
len_uv  = 12 * num_uvw
len_idx = 4 + 8 * num_poligonos
len_xyz = 12 * num_vertices
0 <= indice < num_vertices
```

No se identifican registros por tripletas aleatorias; el parser exige
marcador, longitud que encaje en el archivo, footer observado, controles,
conteos compatibles y validacion de indices.

## Muestra concreta: FemaleHobo.c4d

| Registro | Offset marcador | Offset payload | Tamaño | Conteos |
|---|---:|---:|---:|---|
| UVW 0x85 | 208798 | 208803 | 191232 | 15936 UVW |
| Caras 0x86 | 401860 | 401869 (sin 4 bytes control) | 31876 | 3984 triangulos |
| XYZ 0x85 | 433936 | 433941 | 24300 | 2025 vertices |

Polygon0 = (0,1073,1,1); XYZ0 = (12.70818,-11.77756,0.08734).
El poligono0 utiliza 4 UVW, y UVW2==UVW3; se reproduce en un
OBJ diagnostico con topologia y UV compatibles.

## Otros ejemplos

- MalePunk.c4d: 2264 vertices, 3654 triangulos, 14616 UVW.
- FemaleNurse.c4d: 1820 vertices, 3560 triangulos, 14240 UVW.
- EGF2.c4d: 6509 vertices, 12707 triangulos, 50828 UVW.
- ENP1.c4d: cuatro mallas con 261/1147/250/314 vertices.
- EGF4.c4d: UV fuera de 0..1; fallo del primer validador corregido.

## Auditoria ampliada

`CORPUS_NATIVE_MESH_AUDIT.csv`:

- 93/93 Land of the Dead, 85/85 Umbrella Chronicles, 82/82 Darkside Chronicles.
- TOTAL 260/260 `.c4d` con malla(s) detectadas y **stream completo de
  XYZ, UV e indices leido y validado**.
- 330 mallas, 1051991 vertices, 1990591 triangulos (cuentas por archivo).

Las muestras pueden compartir la misma geometria: no confundir totales
acumulados con cantidad de personajes diferentes.

## Limites

**No hay prueba todavia de jerarquia de objetos/padres, pesos y bind pose,
matrices del scene graph, animacion, asignacion de materiales ni normales
nativas.** Solo hay nombres de objetos y joints aislados mediante cadenas.
El parser tiene alcance experimental para variantes C4D6 comprobadas;
NO significa compatibilidad universal con `.c4d` antiguos/nuevos.
