> Documento histórico de integración Phase11. La versión actual es Phase12.
> Véase `docs/HOD2_PHASE12_STANDALONE_RUNTIME.md`.

# HOD2 Phase11 -> Modelo Runtime 75 (C89, SOURCE ONLY)

## Qué está implementado

El proveedor `src/providers/provider_hod2_89.c` queda registrado en
`modelrt_register_builtin_providers()` con ID **103**, sin modificar las ID de
RE3 V028 (7), Gun Survivor v19 (102) ni los demás proveedores.

- Abre `.pol` o `.h2pol` crudos y POL comprimidos con PRS (prefijo LE32 de tamaño
  descomprimido). Abre `.bin` bajo una carpeta `pol/`, únicamente después de
  verificar por completo el contenido.
- Valida estructura de modelos usando **hod2_pol_model/hod2_pol_scan** de los
  fuentes originales Phase11, no un supuesto formato universal.
- Emite triángulos mediante el modelo de visitantes del runtime. Conserva
  coordenadas, normales y UV originales (IEEE754 -> punto fijo Q16), materiales
  sin inventar texturas y winding original de strips/triangle lists.
- Reconoce `.h2mc` versión 1, metadatos de tipo/MOT, cantidad de huesos,
  fotogramas y ticks. Marca la animación como **UNBOUND**: los datos de un H2MC
  suelto no son suficientes para reconstruir o enlazar el personaje.
- El núcleo usa el espacio de trabajo proporcionado por el llamador; no
  reserva heap ni incluye modelos. Las fuentes originales Phase11 completas
  permanecen en `vendor/hod2_phase11/native_c89/` y
  `vendor/hod2_phase11/inherited/c89/`. Incluyen `skincharacter`, `skinpart`,
  tablas EXE, POL, TEX, MOT y deformación CharacterDrawBonePart.
- Evita enlazar los espacios de trabajo estáticos de 128 MB del CLI original
  contra la biblioteca del runtime. `hod2_binary_runtime.c` incluye solamente
  las primitivas requeridas por el POL y descompresión PRS en el arena.

## Importante: límites de integración

**No** equivale a reproducir HOD2 Phase11 entero directamente desde el visor:

1. El nuevo proveedor abre **POL a nivel de geometría** sin EXE, y **H2MC a
   nivel de catálogo**; no monta automáticamente el esqueleto desde PE ni
   aplica transformaciones MOT sobre el cuerpo en el visitor principal.
2. Para deformaciones reales, texturas TEX y animación, el ejecutable auxiliar
   `vendor/hod2_phase11/native_c89/hod2c89` sigue siendo la ruta completa.
   Necesita EXE original del perfil y bancos POL/TEX/MOT externos, que **NO**
   se incluyen.
3. Los formatos POL alternativos son rechazados explícitamente en vez de
   presentar mallas rotas.
4. TEX necesita tablas EXE y paletas; el proveedor básico deja
   `texture_count=0`. El CLI Phase11 puede incorporarlas en sus GLB.
5. No se prueban aquí los 86 tipos ni modelos reales de versiones 1999/2000.
   La cola del tipo 23 y referencias incompletas señaladas por Phase11 siguen
   sin solución.
6. No se ha probado MinGW32 real; solo GCC C89 en Linux.

## Ruta de personaje completo usando el códec Phase11 incluido

Desde `vendor/hod2_phase11/native_c89`:

```sh
make CC=gcc
./hod2c89 motdump 1999 "Hod2 - y99.exe" "mot" 15 187 "tipo15_mot187.h2mc"
./hod2c89 skincharacter 1999 "Hod2 - y99.exe" 15 "pol" "tipo15_mot187.h2mc" "personaje15.glb" "tex"
```

Después, el proveedor glTF que ya existe en el Runtime 75 puede abrir ese GLB.
No confundas esta ruta de exportación y reapertura con un puente nativo
MOT/EXE/TEX en tiempo de ejecución: **ese puente todavía falta**.

Para Windows MinGW32 usar el `build_win32.bat` incluido con Phase11 o ejecutar
`mingw32-make CC=gcc` en la carpeta nativa.

## Compilar y comprobar

```sh
make -j4
make hod2-synthetic
make hod2-tool-check
make gunsurvivor-synthetic
make re3-regression
make native-iqm-interpolation-smoke
```

`hod2-synthetic` crea POL crudo, POL comprimido PRS y H2MC ficticios (sin
contenido comercial). El programa de prueba comprueba la API pública,
los materiales, los triángulos y que no se invente reproducción MOT.

## Procedencia

Fuente: los dos ZIP Phase11 proporcionados por el usuario; todos los archivos
C89 relevantes del paquete nativo original se conservan sin modificaciones en
`vendor/hod2_phase11`. Se incluye licencia `HOTD2RE_LICENSE.txt` y documentos
Phase11 del paquete original. Adaptación del wrapper bajo el mismo contrato
C89, sin `malloc/calloc/realloc/free`, ni `float/double`, ni `stdin`.
