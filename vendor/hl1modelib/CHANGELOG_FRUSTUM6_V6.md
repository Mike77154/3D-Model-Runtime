# GoldSrc Frustum6 v6 — changelog

- NUEVO `hlfp_frustum89.[ch]`: recorte contra seis planos con estructura cámara + viewport.
- API `hlfp_f89_from_viewport`, `hlfp_f89_clip_triangle`, `hlfp_f89_clip_segment`.
- NUEVOS switches `full_frustum_clipping`, `perspective_far_z` en `hlfp_soft2_options`.
- Integración en render mallas y en esqueleto.
- Cola alfa: culling anticipado de caras fuera del frustum y sorting por porción visible.
- Preparación reutilizable del frustum por malla/pase, en lugar de por triángulo.
- Eliminación condicional del límite v5 +/-4096 px después de un clipping v6 validado.
- `soft2_draw_line`: reemplazado desplazamiento izquierdo de entero potencialmente negativo por producto definido.
- Fuentes, pruebas, demos y reportes agregados; no se elimina ningún módulo v5.
- C89 estricto en GCC Linux: 13 pruebas, 822/822 casos de corpus sin errores, 20k triángulos de estrés.
