# v7 — Subpixel / Top-left

- Nueva API `hlfp_sp89_prepare()` + `hlfp_sp89_sample()`.
- Coordenadas 28.4 y muestreo en centros; regla de media abertura orientacion-invariante.
- Propagacion de error de overflow al renderizador a traves de `hlfp__set_error`.
- Ruta v6 mantenida como predeterminada (`subpixel_top_left = HLFP_FALSE`).
- Integracion de UV corregidas por perspectiva, Gouraud, z-buffer y materiales.
- Pruebas del cuadrado, cambios de winding, aristas T, 256 casos deterministas,
  imagenes binarias AB/BA, corpus completo y render de zombie real.
- Sin asignacion dinamica / coma flotante / enteros de 64 bits en el nuevo modulo.
