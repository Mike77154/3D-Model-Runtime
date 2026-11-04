# Resident Evil: Gun Survivor — Codec C89 integral v19

## Alcance REAL

Este paquete contiene **cuatro programas independientes en ANSI C89**, sin malloc/calloc/realloc/free, sin heap gestionado por la aplicación, sin `float`/`double` en el código y sin lectura por `stdin`:

| Programa | Entrada | Salida | Qué hace |
| --- | --- | --- | --- |
| `gsnab` | `.NAB` | `.obj`, `.mtl`, `.tga`, `.parts.csv`, `.coords.csv` | Reconstruye 31 variantes de geometría, atlas por descriptor/plano, UV en convención OBJ, normales y pose base mediante jerarquía nativa con aritmética Q8/Q12. |
| `gsnacdump` | `.NAC`, número de pistas | `_clips.csv`, `_keys.csv` | Extrae las 703 secuencias disponibles de las 31 variantes, referencias y campos numéricos de claves; admite cabeceras TC y sin TC. |
| `gshmd` | `.HMD` de Tyrant + `.tim` | `.obj`, `.mtl` | Extrae 156 vértices, 214 polígonos / 300 triángulos de geometría HMD especial, UV por páginas TPage y normales. |
| `gstim` | `.tim` PSX RGB555 (16-bit) | `.tga` | Convierte canales PlayStation RGB555 sin intercambiar rojo y azul. STP no se interpreta como transparencia automática. |

Compilado y probado en Linux con **`gcc -std=c89 -pedantic -Wall -Wextra -Werror -O2`**; scripts incluidos para compilar en tu **MSYS2/MinGW32** de Windows. **No incluimos un `.exe` Windows precompilado porque aquí no hay un compilador cruzado Win32.**

## Inicio rápido en Windows

1. Extrae el ZIP completo manteniendo las carpetas `source`, `inputs`, `models` y `outputs`.
2. Desde la consola **MSYS2 MinGW32** ejecuta `BUILD_WIN32.bat` o ábrelo en el entorno donde `gcc` sea accesible. Si prefieres, ejecuta directamente las cuatro líneas de GCC que contiene.
3. Ejecuta `EXPORT_ALL_WIN32.bat` para volver a procesar los 31 pares NAB/NAC y los dos HMD.
4. Abre cualquier archivo `outputs/ENEMxx.obj` en Blender u otro visor de OBJ con soporte de materiales `.mtl` y texturas `.tga`.
5. Para ver las animaciones de demostración, abre `models/ENEMxx_ALL_CLIPS_v19.glb` en un visor glTF que permita elegir animaciones.
6. Para Mr. X de cuerpo completo abre `models/ENEM22_FULLBODY_v19.glb` o `ENEM23_FULLBODY_v19.glb`.

Ejemplo en consola:

```bat
bin\gsnab.exe inputs\ENEM13.NAB outputs\ENEM13
bin\gsnacdump.exe inputs\ENEM13.NAC 19 outputs\ENEM13
bin\gshmd.exe inputs\TAIRANT.HMD inputs\tairant.tim outputs\TAIRANT
bin\gstim.exe inputs\tairant.tim outputs\TAIRANT.tga
```

Los nombres de archivos se conservan sin requerir librerías externas de render, Win32 ni DirectX.

## Cambios integrados respecto a v16/v18

**UV Licker:** la matriz UV dentro del GLB de la v16 **ya coincidía con la del NAB original**: comprobado accediendo directamente a los búferes `TEXCOORD_0`. El propio exportador trimesh había compensado la inversión de V. No se vuelven a invertir esos UV. El atlas incrustado corresponde byte a byte a los píxeles recuperados del NAB.

**Mr. X:** la malla HMD inferior se exportaba directamente, no mediante trimesh. En la v18 su V sí estaba invertida para glTF, y esta v19 la corrige únicamente en esa malla. El material conserva el RGB555 de PlayStation corregido de v18. El cuerpo completo mantiene los **468 triángulos NAB + 300 HMD = 768** y los **22 o 23 clips** de ENEM22/23.

**Caras blancas / agujeros:** los GLB v19 activan materiales `doubleSided` para facilitar la inspección de superficies abiertas y eliminan el oscurecimiento artificial que causaba el `baseColorFactor=[0.4,0.4,0.4,1]` previo. Esto mejora la representación pero **NO cierra agujeros geométricos ni verifica la unión HMD-NAB**. La malla inferior HMD sigue teniendo bordes abiertos, propios de esta extracción; la articulación exacta de la cintura no se ha validado contra el juego ejecutándose.

**Poses OBJ C89:** los OBJ nuevos ya no se exportan como piezas locales sueltas. Se aplica la cadena de matrices `R @ T @ parent` de la pose nativa usando punto fijo Q8/Q12. La posición ya sale en metros, con eje Y vertical; se exportan también normales de vértice, UV y materiales. Las diferencias de punto fijo medidas con el prototipo de referencia son menores que 1.1 mm en los modelos de prueba ENEM00, ENEM13, ENEM22, ENEM31 y ENEM33. En `outputs/ENEM31/32/33` se **conservan** también los grupos asociados a la raíz: no se borran piezas a ciegas.

**NAB multitek:** los grupos enlazan descriptor + índice de imagen local; el códec admite todas las 50 imágenes de textura de los 31 NAB, incluidos los 17 planos de ENEM33 (dos descriptores).

**NAC:** el lector C89 produce un catálogo sin inventar FPS. Las marcas `tag_low` son referencias/campos de control originales, no un reloj confirmado. Las 703 animaciones incluidas en los GLB tienen **reproducción todavía experimental** (orientación, interpolación, velocidad y eventos), tal y como indica su nombre/metadata.

## Resultados verificados

- 31/31 `NAB` válidos y exportados por `gsnab` en C89.
- 31/31 `NAC` válidos y exportados por `gsnacdump` en C89.
- 31 GLB con 703 secuencias, 17 779 triángulos y 50 imágenes NAB; no se eliminaron triángulos.
- Los GLB y OBJ conservan la asociación por grupo; UV glTF v19 se compararon con las UV binarias de los 31 NAB y coinciden exactamente.
- Los 50 TGA generados por C89 son idénticos píxel por píxel a los atlas PNG recuperados.
- Los 2 GLB de Tyrant de cuerpo completo: 768 triángulos, 22/23 clips, malla HMD inferior 300 triángulos, UV HMD actualizadas.
- PSX TIM RGB555 TGA generado por C89: RGB píxel a píxel idéntico a PNG corregido de v18.

## Carpetas

- `source/`: fuentes C89 auditables.
- `inputs/`: los 31 pares NAB/NAC proporcionados por el usuario y los recursos HMD/TIM asociados a Mr. X.
- `outputs/`: OBJ/MTL/TGA/CSV **ya generados en C89** para los 31 modelos más TAIRANT y TAIRANTB.
- `models/`: 31 GLB con animaciones seleccionables y 2 GLB completos de Mr. X.
- `reports/`: registros de validación y pseudocódigo de referencia recuperado previamente de Ghidra.
- `tools/`: scripts de comprobación y transformación puntual de GLB usados para construir esta entrega. **No son requisitos para ejecutar los lectores C89.**

## Límites que todavía existen

El C89 de esta versión **NO contiene un motor de render interactivo ni un reproductor NAC fiel al juego**. Permite leer/exportar los 31 NAB, leer/exportar las 703 secuencias de los NAC y abrir los OBJ con sus texturas en herramientas 3D; para inspeccionar las animaciones se suministran GLB experimentales ya construidos. Las marcas de eventos, el blending original y las transformaciones runtime del Tyrant siguen pendientes de ingeniería inversa. No se asegura cobertura para otros modelos no incluidos en los 31 pares suministrados.

No incluye redistribución de Ghidra, Psy-Q ni `gun_bio.exe`.
