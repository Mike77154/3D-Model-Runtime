# Fase 14 · catálogo completo de skins, DDS, referencias y metadatos de poses/clips

- Agregados: `gs3d_dds.c`, `gs3d_texture_info`, `gs3d_texture_resolve`, callback de recursos externos y puente de metadatos.
- Corrección: longitud DDS MDL7 puede exceder 65535 bytes; el valor `width` de DDS es tamaño del blob, no dimensiones de la textura.
- DDS compatibles: DXT1/BC1, DXT3/BC2, DXT5/BC3, 32-bit RGB(A) por máscaras; DX10/BC4+/KTX no implementados.
- El visor puede enseñar todas las skins de cada MDL y probar un slot de skin específico sin crear malla convertida.
- El nuevo catálogo conserva todos los nombres de fotograma y calcula prefijos candidatos, sin suponer duraciones originales.
- Pruebas: BC1, BC2, BC3, 32-bit, alias, callback externo, DDS inválido y truncado; regresiones de Fase 12 y 13 verificadas.
- Pendiente: poses esqueléticas auténticas contra Acknex; interpretación de materiales complejos, texturas externas no accesibles y formatos DDS adicionales.
