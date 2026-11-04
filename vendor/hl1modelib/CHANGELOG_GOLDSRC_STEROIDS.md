# Changelog GoldSrc steroids

## 2026-06-19

Agregado:

- `hlfp_goldsrc.h`
- `hlfp_goldsrc.c`
- `test_hlfp_goldsrc.c`
- Funciones para companion filenames `T.mdl` y seqgroups `01.mdl`.
- Bodygroup evaluator/setter.
- Skin family resolver.
- Texture paletted -> RGBA32.
- Bind pose generator.
- Mesh baking con matrices de hueso.
- OBJ export de bind pose o secuencia/frame.
- QC scaffold writer.
- Reporte diagnóstico.

Mejorado:

- Makefile ahora compila `hlfp_goldsrc.c` dentro de `libhlfp.a`.
- `test_output.txt` incluye smoke test original + smoke GoldSrc steroids.
- Inicialización explícita de UV temporales en `hlfp_compile.c` para evitar warnings conservadores de GCC.

Verificado:

- `make clean all test_output.txt`
- `-std=c89 -Wall -Wextra -pedantic -O2`
- Sin warnings.
- Sin `malloc/calloc/realloc/free`.
- Sin tipos `float`/`double` en runtime.
