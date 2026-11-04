# Informe de pruebas del port C89 (Phase08)

## Compilación

Comandos ejecutados con éxito en el entorno Linux de desarrollo (no se ha ejecutado bajo MSYS2 Win32):

```sh
make clean all check
clang -O2 -std=c89 -pedantic -Wall -Wextra -Werror \
  character_draw_bone_part.c hod2_binary.c hod2_tables.c \
  hod2_pol.c hod2_tex.c hod2_mot.c hod2_glb.c \
  tests/test_native.c ../inherited/c89/hod2_resource_guard.c \
  ../inherited/c89/hod2_extra_parts.c \
  ../inherited/c89/hod2_character_clips.c -o test_native_clang
./test_native_clang
```

Salida GCC/Clang: `HOD2 C89 native PASS`.

El entorno también ejecutó pruebas sintéticas previas con `-fsanitize=address,undefined` sin alertas. Los tests C89 existentes `test_guard` y `test_character_clips` pasaron; la tabla de clips cubre las referencias registradas de ambas versiones, no constituye una prueba de reproducción real de animación.

## Comprobaciones con recursos sintéticos

| Prueba | Resultado |
|---|---|
| PRS de dos bytes | `AB` recuperado |
| POL con 1 material, 1 triángulo y 3 vértices | Parseo correcto, UV finitas |
| TEX RGB565 2×2 | PNG 2×2 emitido y abierto |
| glTF 2.0 GLB de malla aislada | Cabecera/chunks y accesor POSITION con `min`/`max` |
| GLB de malla con TEX compatible | PNG embebido y referenciado por material |
| PE32 sintético y un nodo | Esqueleto e ID de modelo recuperados |
| Banco MOT sintético | 1 clip enumerado y bloque H2MC exportado |
| `peprobe` | Listado de direcciones y bytes del archivo sintético |

La fixture PE32 sintética **no es una copia de un ejecutable comercial**; la fixture de geometría también es sintética.

## Qué no pudo verificarse

El ZIP suministrado no contiene los ejecutables originales y bancos POL/MOT/TEX del juego. No se efectuó en este port una regeneración de los 172 GLB ni auditoría visual exhaustiva de todos los enemigos. Tampoco se verificó el ejecutable Win32 por falta de compilador i686 MinGW en este entorno. Los reportes heredados son documentación de fases previas, no nuevas pruebas sobre este port.

## Pruebas Phase08 añadidas

- `make clean all check` — GCC C89, **pasa** los tests de base y `test_character` con 2 huesos, POL y un H2MC de 4 muestras a 30 Hz.
- `clang -std=c89 -pedantic -Wall -Wextra -Werror ...` — CLI y pruebas sintéticas, **pasan**.
- Clang `-fsanitize=address,undefined` y GCC `-fsanitize=address,undefined` sobre `tests/test_character.c`, **sin alertas** en la versión final.
- Material PNG de la fixture embebido en GLB, `trimesh` carga tanto la variante con textura como la variante sin textura.
- Comparador externo SciPy: error máximo absoluto por componente de quaternion `1.449603e-05` en el ejemplo sintético de 90°; tiempos 0, 1/30, 2/30 y 3/30 en IEEE754 binary32 corresponden al float32 de referencia.
- `batch` con PE fixture que solo define tipo 0: 1 exportación válida y 85 fallos explícitos con CSV de 86 filas, como se espera.
- BSS de CLI en Linux después de compartir workspaces: **248,415,040 bytes (~237 MiB)**, sin heap; no es una medición de ejecución Win32.

**Limitación:** la comparación externa usa Python solo como oráculo de prueba, no como dependencia del programa C89. El test de dos huesos no comprueba piel ni deformación anatómica en datos originales.
