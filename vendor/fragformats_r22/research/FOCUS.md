# Ghidra — rutas específicas para siguiente revisión

**Archivo fuente:** `fragMOTION.exe` (idéntico al del rar). Pasada estática con objdump (sin ejecución).

| Archivo | VA | Interés |
|---|---:|---|
| `fragMOTION.exe` | `0x0046B621` | Exportador UGH, genera el prefijo `ugh0` y escribe cabecera de 16 bytes. |
| `fragMOTION.exe` | `0x0046C0D0` | Siguiente llamada del exportador UGH tras escribir la cabecera; investigar jerarquía de contenido. |
| `fragMOTION.exe` | `0x00456218` | Lector ARG: versión ≤0x00010002, compara marca `arg00`. |
| `FileTypeXMDL.dll` | `0x10001020` | Registro de handlers importer/exporter. |
| `FileTypeXMDL.dll` | `0x10004630` | Función grande con `switch` de códigos internos; no se ha reconstruido la gramática. |
| `FileTypeXMDL.dll` | `0x10007680` | Función demasiado grande para decompilación breve; requiere segmentación. |
| `FileTypeDBO.dll` | `0x100058B0` | Registro de handlers importer/exporter. |

Los informes completos de análisis parcial pueden consultarse en el ZIP separado `FragFormats_Ghidra_R1_Analysis.zip`. Ningún archivo `.xmdl` auténtico fue incluido en la aportación; por eso el lector XMDL aún es únicamente una sonda hexadecimal.
