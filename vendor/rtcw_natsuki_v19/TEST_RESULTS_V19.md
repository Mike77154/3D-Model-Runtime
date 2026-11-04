# RTCW Natsuki Codec v0.19 — resultados reproducibles

Fecha: 2026-11-02

| Comprobación | Resultado |
|---|---|
| `make CC=gcc -j3 all`, C89, `-Wall -Wextra -Wpedantic -Werror` | PASS: 18 programas |
| `make CC=clang -j3 all`, mismos indicadores | PASS: 18 programas |
| `RUN_ALL_TESTS.sh` bajo GCC | PASS: 20 suites, 715 comprobaciones (ejecución final GCC) |
| `RUN_ALL_TESTS.sh` bajo Clang | PASS: 20 suites, 708 comprobaciones en ejecución completa anterior; 65 de v0.19 verificadas por separado |
| Nuevas comprobaciones añadidas tras auditoría UV | PASS: 65 en v0.19 |
| GCC UBSan: `rtcw_audit`, 65 pruebas | PASS, sin diagnósticos |
| Compilación Win32 MSYS2 | PENDIENTE |
| Modelos comerciales RTCW | PENDIENTE |

El recuento final GCC es de **715 comprobaciones** (650 heredadas + 65 de v0.19).
Las 65 nuevas también pasaron con Clang y bajo GCC UBSan. El total completo
de Clang se comprobó antes de añadir los siete casos finales de UV, sin alterar
los módulos heredados.

Se evita equiparar `WARN` con corrupción demostrada. En particular, UV constantes,
materiales no hallados y triángulos degenerados requieren revisión de contexto.
