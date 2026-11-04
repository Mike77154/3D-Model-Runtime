# HOD2 Phase12 integrado en Modelo Runtime 75 (C89, SOURCE ONLY)

## Cambio respecto a Phase11

**Nuevo:** sin ejecutable PE en tiempo de uso. Los `.h2tb` (1999 y 2000)
contienen los segmentos originales de datos inicializados, con direcciones
virtuales para que el motor C89 resuelva esqueletos, piezas deformables, referencias
a modelos, bancos de movimientos y descriptores de texturas. No contienen los
modelos POL, texturas TEX ni fotogramas MOT ni sección de código `.text`.

El proveedor **103** ahora admite `.h2tb` junto a `.pol`, `.h2pol`, `.bin` de
carpeta `pol` y `.h2mc`. No cambian las IDs RE3=7 o GunSurvivor=102.

Se reemplazó `vendor/hod2_phase11` por `vendor/hod2_phase12` y se incorporaron
los H2TB originales que aportó el usuario junto con sus CSV de auditoría;
no se integraron modelos de muestra ni se incluyen recursos POL/MOT/TEX.

### Runtime: consulta explícita de esqueletos

```c
#include "model_runtime89.h"
#include "modelrt_hod2_phase12.h"

/* Tras inicializar runtime/handle, y cargar .h2tb:
 * modelrt_open_file(&rt, &handle,
 *    "vendor/hod2_phase12/tables/HOD2_1999_tables.h2tb");
 */
modelrt_hod2_select_character(&handle, 15u);
/* modelrt_get_info(&handle)->bone_count ahora corresponde al tipo 15.
 * modelrt_visit(&handle,MODELRT_ANIM_BIND_POSE,0,&sink,context);
 * emite on_bone() por cada nodo.
 */
```

Nuevas funciones en `include/modelrt_hod2_phase12.h`:

- `modelrt_hod2_table_profile()` (0=1999, 1=2000).
- `modelrt_hod2_select_character()` (tipo 0..85): activa el esqueleto
  para la visita nativa `on_bone()`, en Q16 y sin inferir otras mallas.
- `modelrt_hod2_extra_part_count()` y `modelrt_hod2_extra_part_get()`:
  devuelven registros de piezas adicionales, huesos asociados,
  `draw_mode`, número de vértices y conteos de filas cuando existen.

Sin selección de tipo, H2TB no reporta un esqueleto único; eso es intencional.
El visor no puede mostrar una malla de personaje a partir de tablas solas.

### Toolkit C89 Phase12 vendorizado

Desde `vendor/hod2_phase12/native_c89/`:

```sh
make CC=gcc
./hod2c89 tablescheck 1999 ../tables/HOD2_1999_tables.h2tb
./hod2c89 skeleton 1999 ../tables/HOD2_1999_tables.h2tb 15
./hod2c89 parts 1999 ../tables/HOD2_1999_tables.h2tb 15
```

Para **personajes animados con abdomen deformable** siguen siendo necesarios
los directorios del usuario `pol/`, `tex/`, `mot/` y los H2MC derivados de MOT.
Con ellos, el toolkit nativo puede ejecutar `motdump` y `skincharacter`
utilizando `.h2tb` en lugar del EXE (véase `README_PHASE12.md`).
La deformación `CharacterDrawBonePart` está en el toolkit Phase12, pero
**NO hay aún enlace completo POL+MOT+TEX+H2TB dentro del visitor principal del
runtime**. `H2MC` sigue etiquetado como animación sin enlazar (`UNBOUND`).
La ruta actual para ver un personaje articulado completo es exportar GLB
con el toolkit nativo, después abrir el GLB desde el runtime.

### Compilación y regresiones

```sh
make -j12 libmodel_runtime89.a
make hod2-phase12-tables
make hod2-synthetic hod2-tool-check
make gunsurvivor-synthetic re3-regression native-iqm-interpolation-smoke
```

Comprobaciones realizadas en Linux con GCC C89:
- Binario de biblioteca completo: PASS.
- Nuevo proveedor y puente aislados con `-std=c89 -pedantic -Wall -Wextra -Werror`: PASS.
- 86 esqueletos inspeccionados por perfil, 172 en total: PASS.
- Integridad de perfiles H2TB y de FNV32 validada, perfil incorrecto/corrupción rechazados: PASS.
- Pruebas nativas `test_native` y `test_character` y POL/PRS/H2MC sintéticos: PASS.
- Regresiones RE3 V028, Gun Survivor V19, IQM: PASS.

**No se ha comprobado compilación i686 MinGW32 ni calidad visual de los 86
personajes reales; no se debe interpretar este informe como una certificación
de animaciones completas.**
