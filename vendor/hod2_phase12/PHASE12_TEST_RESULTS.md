# Phase12 — resultados de pruebas

## Fuentes y paquetes
- 1999: `Hod2 - y99.exe` (1,699,840 bytes) --> `HOD2_1999_tables.h2tb` (888,880 bytes).
- 2000: `Hod2 - y2000.exe` (1,695,744 bytes) --> `HOD2_2000_tables.h2tb` (888,880 bytes).
- Se conservan `.rdata` (724,992 bytes) y `.data` (163,840 bytes), mas encabezado H2TB (48 bytes).
- No se copian secciones `.text` ni `.rsrc`.

## SHA256
- 1999 `dc2458260be32eb6f6dc569fe618cb6e0e08a7dcaf31b8dab127774307a91ddd`
- 2000 `d091929ad93400a75f502c257f2ce8fc3dd238cdec7651bee2c94eee5cc4ea79`

## Pruebas con los datos reales
- 1999: skeleton 86/86, parts 86/86, siete pruebas de funciones comparadas.
- 2000: skeleton 86/86, parts 86/86, siete pruebas de funciones comparadas.
- Total: 358/358 comparaciones sin diferencias.
- Las pruebas auxiliares incluyen skincharacter real tipo 15 y su GLB identico al generado con EXE.
- CSV exportados de ambos orígenes: 5/5 por versión, idénticos.
- `tablescheck` rechaza perfiles cruzados, datos alterados y truncamientos.
- GCC C89 / Clang C89 / suite `make check`: aprobados.

## Alcance y limitaciones
- Los archivos .h2tb eliminan la dependencia del ejecutable solamente para
  consultar los datos presentes en las secciones extraídas.
- `peprobe` es una utilidad de inspección del PE y, por diseño, no puede
  recuperar instrucciones o rutinas de código desde un paquete de tablas.
- Se siguen necesitando los bancos POL, TEX y MOT para renderizar y animar.
- El código debe ser el lector actualizado de Phase12, no el antiguo Phase11.
- El ejecutable Win32 no se ha compilado ni ejecutado sobre Windows en esta prueba.
