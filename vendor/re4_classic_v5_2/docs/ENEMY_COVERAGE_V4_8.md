# RE4 Classic Codec v4.8 — Reparación real de rigs y revisión de anomalías

**Fecha:** 2026-11-02. **Corpus:** `xfile.dat` y `em.dat` de RE4 PC 2007 facilitados por el usuario.
**Política:** C89 estricto en la librería, Q16.16, sin asignación dinámica, archivos de juego excluidos del ZIP.

## Cambios verificados en el código

1. `animation/re4_character_assembler.c`: los `born1__NN` y `born2__NN` de un PMD principal ya pueden recibir enlaces de una pieza. Solo se aceptan nombres reales `born__`, `born1__` y `born2__`; nunca se interpretan como identificadores válidos prefijos arbitrarios. Identificadores duplicados de maestro provocan error de formato en lugar de un enlace silencioso.
2. `animation/re4_skeletal_player.c`: `re4_skel_joint_for_fcv_id` ahora reconoce `born1__` y `born2__`, con rechazo de ambigüedades y sufijos malformados. Esto desbloqueó las rotaciones FCV de la armadura `em3c00`.
3. `examples/re4_mesh_sequence_audit.c`: la alerta de desplazamiento normaliza su umbral por el intervalo entre muestras (`step`), sin borrar el desplazamiento total del reporte. **Un muestreo disperso no puede garantizar ausencia de cambios bruscos entre las poses omitidas.**
4. `tests/re4_born1_master_test.c`: prueba sintética C89 del rig `born1__`, de las referencias a sus piezas y del rechazo seguro de identidades duplicadas.
5. `examples/re4_v47_mesh_corpus.py`: `mendez_phase_b` se evalúa con `--review-ancestry` explícito en lugar de fallar por una política de jerarquía imposible para esa combinación; **no es prueba de un ensamblaje canónico**.

## Pruebas sobre recursos auténticos

| Caso | Modelos principales | FCV probados | Resultado técnico |
| --- | --- | --- | --- |
| Armadura A | `em3c00` + `em3c01` | 55 de 55, muestreo | 31 enlaces directos, 7,098 vértices/pose, 55 PASSED |
| Armadura A, completo | mismas dos piezas | FCV 25, 129 fotogramas | PASSED, sin alertas |
| Méndez fase 2 | `em3501a` + `em3501b` | 83 de 83, muestreo | 76 enlaces directos y 39 diferencias de jerarquía; 83 PASSED bajo política de revisión |
| Méndez, completo | mismas dos piezas | FCV 19, 149 fotogramas | PASSED con política de revisión, sin alertas |
| Configuraciones multipartes v4.7 | 19 candidatas explícitas | FCV seleccionado por candidato | 19 de 19 pruebas técnicas PASSED |
| Salazar mutado | `em3800` | FCV 42, 251 fotogramas consecutivos | Desplazamiento máximo de ~141.59 unidades entre dos fotogramas (umbral 250), sin alertas |

**Importante sobre Méndez:** en el render VTK del FCV 19, la malla `em3501b` (parte inferior/ropa) se aprecia físicamente separada del torso del jefe. Dicha separación puede formar parte de la transición o de un elemento independiente de escena; los eventos de transformación y posición no están reconstruidos. No etiquetar este conjunto como «ensamblaje visual canónico». Dejar como candidato FK que requiere investigación de eventos SEQ y jerarquía original.

## QA reproducible

- `make clean && make -j8 all && make check`: **29 suites C89** superadas.
- La misma batería, con `-fsanitize=address,undefined -fno-omit-frame-pointer -O1`: superada para código y pruebas sintéticas.
- El Python de orquestación es opcional y no forma parte de las bibliotecas vendorizadas C89.
- `re4_character_family_preview.py` exportó OBJ de dos poses para cada familia y las renderizó con VTK; 47 actores para Armadura A, 55 para Méndez fase 2.
- **No** se ha probado un ejecutable Win32 nativo ni se ha reproducido exactamente el IK, movimiento raíz, estados de daño y eventos SEQ del original.

## Reproducir sin suponer que los recursos están incluidos

```sh
./re4_mesh_sequence_audit em3c00.pmd em3c_25.fcv 0 128 1 em3c01.pmd --summary
./re4_mesh_sequence_audit em3501a.pmd em35_19.fcv 0 148 1 em3501b.pmd --review-ancestry --summary
./re4_mesh_sequence_audit em3800.pmd em38_42.fcv 0 250 125 --summary
```

Los PMD/FCV se deben extraer de un ejemplar propio de RE4 PC 2007. Las capturas y CSV de verificación se entregan en ZIP separado.
