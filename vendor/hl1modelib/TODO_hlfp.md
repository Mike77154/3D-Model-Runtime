# TODO HLFP

## Hecho en esta rama
- [x] regeneración binaria sin depender del buffer original
- [x] blend temporal con interpolación entre frames
- [x] multipose con quaterniones
- [x] support básico de controllers para seq blend y bone controllers aditivos
- [x] re-emisión más semántica de `main` MDL
- [x] re-emisión más semántica de `T.mdl`
- [x] re-emisión más semántica de `IDSQ`
- [x] re-encodeo de animación desde poses decodificadas
- [x] canonicalización de `tri cmds` a triángulos explícitos

## Pendiente
- [ ] recompilar de vuelta a MDL con fidelidad todavía más alta que esta re-emisión semántica
- [ ] regenerar semánticamente `automove*` en vez de arrastrarlos como opacos
- [ ] preservar strips/fans originales en vez de canonizar a tris explícitos
- [ ] writer completo de QC
- [ ] renderer software raster completo
- [ ] soporte exhaustivo de rarezas oscuras de `studiomdl`
- [ ] seq blend > 4 casos raros y controladores exóticos
- [ ] sound tables / sound groups reales
