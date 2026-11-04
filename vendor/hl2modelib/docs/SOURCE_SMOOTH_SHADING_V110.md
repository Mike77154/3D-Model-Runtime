# v1.0.10 - Smooth shading sobre normales nativas de Source

La malla original conserva **normales y tangentes VVD**. El rasterizador Python del ejemplo v1.0.9 promediaba RGB de sus tres vertices y rellenaba cada triangulo de un color plano. Por ello las capturas se veian facetadas pese a disponer de normales nativas.

Nuevo modulo compilado independiente: `vendors/source/shaderer/hl2m_smooth89.c`. La API `hl2m_master_smooth_fragment89` interpola normal, tangente y UV en Q16.16 con coordenadas baricentricas por pixel (suma 65536) y pasa las muestras a `hl2m_master_fragment89`. La funcion ya existente normaliza y ortogonaliza N/T; luego usar `hl2m_master_shade89` con las luces del presenter. Respeta material por triangulo, signo de tangente y fronteras UV de la malla ya dividida.

No inventar normales promedio globales: romperia filos de la gabardina y costuras. El sombreado suave solo afecta **interiores de triangulos**; no agrega poligonos ni elimina aristas del contorno. El modo de preview v110 usa interpolacion de colores (Gouraud) sobre la salida de shading v109, una aproximacion mas simple que evaluar el mapa normal por pixel. Por ello sigue pendiente actualizar el renderer GPU/presenter externo.

No se usan floats/doubles/int64/malloc dentro de la biblioteca. El preview opcional usa NumPy y OpenCV y no forma parte de la biblioteca.
