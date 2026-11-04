# Distribución v19 — SOLO CÓDIGO FUENTE

Se ha filtrado la distribución v19 original **sin modificar ningún archivo conservado**.

Se eliminaron:
- Ejecutables/binarios compilados (si estaban presentes), archivos `.bin` de prueba y temporales.
- Imágenes, vistas previas, texturas y ejemplos PNG/JPG/BMP/TGA/DDS/VTF/TIFF/PPM/PGM y demás formatos gráficos.
- Modelos, esqueletos, archivos de animación y paquetes de materiales de ejemplo, incluidos FBX, MDL, MS3D, DTS, PSK/PSA, SMD y XPS.
- Registros `.log` y archivos de respaldo `.bak`.

**No se ha eliminado ningún `.c`, `.h`, `.inc`, Makefile, script, licencia o documentación de código.**
El runtime y el visor conservan los 14 proveedores vendorizados de la v19.

**Aviso:** Las pruebas que cargan archivos de muestra existentes bajo `tests/`, `samples/`, `fixtures/` o corpus de proveedores requieren restaurar/regenerar esas muestras por separado. La eliminación no convierte esas pruebas en tests autocontenidos. Las pruebas que generan sus fixtures en ejecución conservan su código.

No se ha modificado el funcionamiento de los decodificadores: simplemente se retiraron los archivos de demostración y los productos generados.

## Inventario de la limpieza

| Grupo | Archivos retirados | Bytes sin comprimir |
|---|---:|---:|
| Binarios compilados | 0 | 0 |
| Imágenes/texturas | 104 | 8648891 |
| Modelos/animaciones | 232 | 397253 |
| Logs/binarios de prueba/temporales | 113 | 165258 |

Retirados: **449**. Conservados del original: **1305**.
