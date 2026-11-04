**v0.17:** consulta [TEST_RESULTS_V17.md](TEST_RESULTS_V17.md). 17 herramientas C89; 18 suites; 631 comprobaciones PASS (fixtures sintéticos).

# v0.16: consultar [TEST_RESULTS_V16.md](TEST_RESULTS_V16.md) para el resultado verificado de la nueva versión.

# v0.15 — Pruebas ejecutadas (2026-11-02)

- `make CC=gcc -j2`: **15 binarios Linux** compilaron en **C89 estricto**, `-Wall -Wextra -Wpedantic -Werror` — PASS.
- `make CC=clang -j2`: **15 binarios Linux** compilaron con las mismas opciones — PASS.
- Se ejecutaron **16 suites Python** `test_codec.py` y v0.02–v0.15, incluido `v14_images` — todas PASS.
- `test_codec_v15.py`: **35 comprobaciones PASS**: frecuencias 2.0 y 0.5 FPS, selección temporal, JPEG/TGA heredados, invalidación de cache al rotar `animMap`, prioridad de VFS, reutilización de extracciones, limpieza de shaders temporales y blending de TGA RGBA semitransparente.
- `test_codec_v15.py` con `rtcw_rig` y `rtcw_matprobe` construidos por Clang — PASS.
- `rtcw_rig` y `rtcw_matprobe` instrumentados con **GCC UBSan**; `test_codec_v15.py` (**35/35**) — PASS sin avisos de UB.
- `test_codec_v14.py` con `rtcw_rig` UBSan (**17/17**) — PASS.
- Pendiente: Win32/MSYS2 real, medir fps en hardware y validar aspecto/rendimiento con modelos originales de monstruos del juego.

---

# v0.14 — Resultados de validación (2026-11-02)

- GCC y Clang: compilación de **14 herramientas** con `-std=c89 -O2 -Wall -Wextra -Wpedantic -Werror`: PASS.
- **13 suites heredadas** v0.1–v0.13, sobre fixtures sintéticos: PASS.
- `test_codec_v14.py`: **17 comprobaciones** PASS, con modelo MDS+MD3, shaders de PK3 y JPEG de mod prevalente, TGA del paquete base, visibilidad y validación de ruta maliciosa.
- `test_codec_v14_images.py`: **86 comprobaciones** PASS, JPEG baseline 444/422/420/gris, modo progresivo rechazado, entradas truncadas/invalidas y 64 variantes mutadas.
- UBSan: el decodificador JPEG y `rtcw_rig` v0.14 con sus pruebas nuevas: PASS sin avisos.
- Los JPEG de test se almacenan como Base64 dentro del código fuente Python (sin Pillow requerido al ejecutar las pruebas).
- NO probado: monstruos comerciales ni compilación Win32/MSYS2 real; fidelidad de shaders complejos.

---

# v0.13 — Pruebas ejecutadas

- `make -j4 all` (GCC, `-std=c89 -Wall -Wextra -Wpedantic -Werror`): **13 objetivos compilados**.
- `make CC=clang all` (mismas advertencias estrictas): **13 objetivos compilados**.
- Pruebas de las versiones v0.1 a v0.13: **13 suites aprobadas** con corpus sintético.
- `python3 tests/test_codec_v13.py ./rtcw_rig`: **24 comprobaciones aprobadas**.
- Compilación de `rtcw_rig` con Clang/UBSan y ejecución de suites v0.12 y v0.13: **ambas aprobadas**, sin reportes del sanitizador.
- **No realizadas:** pruebas con PK3 comerciales, MinGW i686/Windows nativo, paridad exacta con animaciones y shaders originales.

---

# v0.12 test report — 2026-11-02
- Linux GCC 13 executables: PASS (`-std=c89 -O2 -Wall -Wextra -Wpedantic -Werror`).
- Linux Clang 13 executables: PASS (same strict C89 flags).
- Tests for v0.1 through v0.11: PASS (all previous 11 Python suites).
- v0.12 rig + contact + VFS integration: **34** checks PASS under GCC and Clang.
- v0.12 under Clang UBSan: **34** checks PASS.
- Native Windows / MSYS2 mingw32: not executed in this environment.
- Actual commercial RTCW monster model corpus: unavailable, therefore not tested.
- Tests use synthetic test fixtures; build artifacts are intentionally excluded from source archive.

# Test results — RTCW v0.11 (2026-11-02)

Validated with the uploaded v0.10 package as the baseline; original game assets NOT present.

- GCC: all **12** commands via `make -j4 CC=gcc`, strict C89, `-Wall -Wextra -Wpedantic -Werror`: **PASS**.
- Clang: all **12** commands via `make -j4 CC=clang`, same C89 warning policy: **PASS**.
- `tests/test_codec_v11.py`: **74 / 74 PASS** on compiled local executable, synthetic fixtures.
- All inherited scripts `tests/test_codec.py` through `tests/test_codec_v10.py` completed without failures, including v0.10's 139 checks.
- New v0.11 executable built under Clang UndefinedBehaviorSanitizer with `-fsanitize=undefined -fno-sanitize-recover=undefined` and all **74 / 74 PASS**.
- The v0.11 controller does not execute or guess animation conditions. `CHANGE`, `EVENT`, `PLAY` are explicit scenario commands.
- Separate MD3/MDC channel previews are rejected when channel poses differ; full MDS bone-space preview remains enabled.

Not verified: native Win32 build, all real commercial monster models, texture/shader fidelity, engine-exact AI condition evaluation, game-exact script transitions, live rig visibility, sound playback.
