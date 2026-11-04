# QA v0.16 — 2026-11-02

- `make CC=gcc -j2 all` con `-std=c89 -Wall -Wextra -Wpedantic -Werror`: **16 programas PASS**.
- `make CC=clang -j2 all` con mismas opciones: **16 programas PASS**.
- **17 suites** originales y nuevas v0.1–v0.16 en GCC: PASS, total **597** verificaciones contadas por mensajes `PASS`, de las cuales **28** son v0.16 (las 16 anteriores comprenden 569).
- v0.16 con Clang y fixture compilado por Clang: **28 PASS**.
- `rtcw_shaderprobe` y fixture v0.16 con GCC `-fsanitize=undefined -fno-sanitize-recover=undefined`: **28 PASS**.
- `rtcw_rig` con UBSan en regresión v0.15: **35 PASS**.
- Nuevos fixtures incluyen orden invertido de geometría alfa, primeras etapas de shader, cuarto campo opcional VFS, compatibilidad con formato antiguo, alpha masks y segunda etapa registrada como parcial.
- Sin ejecutar: Win32 MSYS2 nativo, recursos originales comerciales RTCW, fidelidad GPU completa.
