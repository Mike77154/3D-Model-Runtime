# Auditoría RE3 para ResidentEvil3.exe, RE3_MERCE.exe y RE3_codec v0.13

**Fecha:** 31 de octubre de 2026.  
**Método:** inspección estática PE/x86 con GNU `objdump` + pruebas reales del código C89 contra el corpus aportado.  
**Límite:** se intentó iniciar REA pero no fue posible utilizar su runtime: `dist/` y `node_modules/` ausentes; Ghidra tampoco está instalado. **NO se ha obtenido pseudocódigo mediante REA/Ghidra, ni se han ejecutado los juegos.**

## 1. Comparación comprobada de los ejecutables

| Atributo | ResidentEvil3.exe | RE3_MERCE.exe |
|---|---|---|
| Formato | PE32 i386 | PE32 i386 |
| Tamaño (bytes) | 1290240 | 1290240 |
| Base | 0x00400000 | 0x00400000 |
| Punto de entrada | 0x004EFC9C | 0x004EFD0C |
| SHA256 | b6b16582d3fbbd2b05d6e76c26a7925fa1952f2d2157a885a6b6675d9290db36 | bc0d29d5cbe19dd36b67e2d9cd8104151240709b9503781699c3ffcd5079b472 |

- 613017 bytes difieren al comparar la misma posición de ambos archivos. **No equivalen a 613017 cambios lógicos:** hay código desplazado; por ejemplo, una rutina aparece con un desplazamiento de +0x50 bytes.
- Diferencias por regiones sin normalizar desplazamientos: `.text` 610515 bytes, `.rdata` 155, `.data` 2342, `.rsrc` 0 (idéntica).
- Ambos importan `KERNEL32.dll`, `DDRAW.dll`, `DSOUND.dll`, `DINPUT.dll`, `WINMM.dll` y otras bibliotecas.

### Primer candidato de cargador EMD/TIM (estático)

Las cadenas y referencias cruzadas comprobadas en la sección `.data` incluyen:

| Recurso | Dirección de cadena | ResidentEvil3.exe referencias | RE3_MERCE.exe referencias |
|---|---|---|---|
| `bio19/room/emd/em10.tim` | `0x0051D740` | `0x004386DA`, `0x004386EF` | `0x0043872A`, `0x0043873F` |
| `bio19/room/emd/em10.emd` | `0x0051D768` | `0x0043873B`, `0x0043874F`, `0x004387AD`, `0x004387B9` | `0x0043878B`, `0x0043879F`, `0x004387FD`, `0x00438809` |
| `bio19/room/emd08/em10.emd` | `0x0051D790` | `0x00438772` | `0x004387C2` |

En la secuencia que comienza aproximadamente en `0x004386B0` (RE3) / `0x00438700` (Mercenarios), la lógica convierte valores de una tabla en punteros sumándoles la dirección base del bloque cargado. Por ejemplo, en RE3, `mov (%edi),%ecx; add %ebp,%ecx; mov %ecx,0x11c(%esi)`, seguido de operaciones análogas para más offsets. Esto demuestra un patrón de resolución de **desplazamientos relativos**, pero aún falta comprobar qué campo semántico representa cada destino y qué rutina concreta produce los píxeles/texturas finales.

**Siguiente investigación x86:** seguir llamadas a `0x00426540` (RE3) y `0x00426590` (Mercenarios), ubicar la estructura receptora en ESI, identificar el tratamiento de cada sección; seguir por separado las rutas de `DDRAW.dll` y las llamadas de conversión de píxeles. Es una hipótesis de trabajo, no un decompilado validado.

## 2. Bug reproducible del códec v0.13

**Síntoma antes del parche:** `re3emd_info EM20.EMD` devolvía `open: static capacity exceeded`.

**Causa aislada mediante instrumentación:** `re3c_parse_all_banks()` intentaba analizar un banco opcional cuya sección `EDD` tenía longitud de **4 bytes** (representación vacía). `parse_edd()` interpretaba su contador de clips cero como un desbordamiento, abortando la apertura del EMD completo. No se trataba de que el archivo completo excediera los buffers de geometría.

**Parche implementado en `src/re3_codec.c`:** `re3c_parse_bank()` devuelve `RE3C_ERR_NOT_PRESENT` cuando la sección EDD opcional tiene longitud de 4 bytes o menos, en lugar de intentar descodificar clips inexistentes.

```c
if (!m->sections[es[id]].present ||
    !m->sections[ms[id]].present ||
    m->sections[es[id]].size <= 4u)
    return RE3C_ERR_NOT_PRESENT;
```

En el corpus se observaron **229 secciones EDD de 4 bytes** entre los 100 EMD. Distribución: EDD A 0, B 40, Damage 41, Weapon A 74, Weapon B 74. Todas las muestras incluyen al menos un banco opcional vacío. El salto de 0/69 a 69/69 al usar una primera colección de nombres únicos quedó confirmado, y después se probaron los 100 recursos completos de la colección original.

## 3. Resultados reproducibles

| Prueba | Fuente sin parche | Fuente parcheada |
|---|---|---|
| Build GCC `-std=c89 -pedantic -Wall -Wextra -Werror -O2` | compilaba | **PASS** |
| Pruebas sintéticas (`make test`) | PASS | **PASS** |
| 69 EMD con nombres distintos | **0/69** | **69/69** |
| 100 EMD del ZIP (incluye variantes EMD/EMD08) | no repetida | **100/100** |
| 100 TIM del ZIP | 100/100 | **100/100** |
| 100 pares EMD+TIM | no repetida | **100/100** |

**Alcance:** aprobar `test_texture_binding` significa que el códec puede atravesar las caras/UV y muestrear las texturas sin errores; **no prueba todavía que el color y render final sean visualmente idénticos al juego original**. La ruta de PLD/PLW tampoco quedó cubierta por esta barrida.

## 4. Prioridades para v0.14

1. Guardar este fix de bancos EDD vacíos y no confundir ausencia opcional con desbordamiento real. Añadir prueba automatizada de regresión.
2. Recuperar y documentar la semántica de los offsets de EMD y de las dos rutas `emd`/`emd08` en los EXE.
3. Montar Ghidra con REA compilado (`npm ci`, `npm run build:cached` en un host compatible) y generar pseudocódigo de las rutinas candidatas; comprobar con trazas de lectura de archivos/depuración en un sistema controlado.
4. Verificar equivalencia exacta de paletas, transparencia, orden BGR/RGB, TPAGE/CLUT, UV y material con texturas exportadas desde la ejecución real.
5. Cobertura independiente de PLD/PLW y formatos de variantes.

## 5. Artefactos adjuntos

- `codec_optional_edd.patch`: diferencia textual exacta sobre v0.13.
- `exe_analysis.json`: metadatos PE, importes y direcciones de las referencias.
- `exe_string_xrefs.csv`: cadenas de recursos y referencias observables de ambos EXE.
- `emd_optional_bank_audit.csv`: longitudes de los cinco bancos EDD para los 100 modelos.
- `*_full.log`: registros originales de pruebas.

Por seguridad y tamaño, **no se redistribuyen los dos ejecutables comerciales ni el corpus**; el ZIP del códec contiene código y documentación, no los binarios del juego.
