# Source SDK 2013 vs hl2modelib — T-103 caminata y causa raíz

## Diagnóstico basado en código y corpus

Comparación hecha contra `src/public/bone_setup.cpp` del archivo SDK aportado:

- `CalcBoneQuaternion`: interpreta `RadianEuler` de las pistas `STUDIO_ANIM_ANIMROT` y construye una rotación legítima (quaternion).
- `CalcBonePosition`: usa `basePos + RLE * posscale`, salvo `STUDIO_ANIM_DELTA`.
- `CalcAnimation`: avanza por `cycle * (numframes - 1)`, interpolación de subframe y pesos óseos.
- `CalcPoseSingle` y `Studio_LocalPoseParameter`: resuelven la tabla de blend de las secuencias.
- `Studio_AnimPosition`, `Studio_SeqMovement`: root motion va a la transformación del actor, no se debe confundir con su esqueleto.

**Causa confirmada y corregida:**
`src/hl2modelib.c:hl2m_fixed_sin_rad` calculaba la serie de Taylor en todo [-pi, pi] sin reducción a cuadrante. Sobre pi, la aproximación erraba ~0.52. La matriz de RadianEuler usada en el camino de animación terminaba no ortogonal. Cambiada a reducción de cuadrante [-pi/2, pi/2] y serie de Taylor hasta grado 9. No usa float, double, memoria dinámica ni 64 bits.

**Regresión medida**: máxima desviación `|norma²(column)-1|` (matriz local, 57, 65 o 85 cuadros del corpus Walk_F):

| Modelo | Antes (Q16.16) | Después (Q16.16) | Antes (aprox.) | Después (aprox.) |
|---|---:|---:|---:|---:|
| Fase 1 | 50257 | 1466 | 0.767 | 0.022 |
| Fase 2 | 59349 | 1738 | 0.906 | 0.027 |
| Fase 3 | 60661 | 1958 | 0.926 | 0.030 |

La norma aún no es perfecta: hay error numérico por las aproximaciones fixed-point y falta redondeo fino. Para reproducción muy precisa se necesitará interpolación de quaternions normalizados, no interpolación directa de Euler.

## Hallazgo de la animación T-103

`TyrantPhase1.mdl` contiene:

- Secuencia `S_Walk` (índice 3), `groupsize = [9, 1]` y `paramindex[0] = 0`.
- Celdas desde izquierda a derecha: `Walk_B`, `Walk_BR`, `Walk_R`, `Walk_FR`, **`Walk_F`**, `Walk_FL`, `Walk_L`, `Walk_BL`, `Walk_B`.
- La celda central 4 selecciona animación interna 1 `Walk_F`, 57 frames, **40 FPS**, `numikrules=0`.
- Los GIF previos presentaban solo 9 frames (cada 7) con 240 ms de pausa, frente a una duración original de 56/40 = **1.400 segundos**; esa velocidad incorrecta y el gran muestreo contribuían a la percepción de balanceo.
- La pista de entidad aporta unos 47.5 unidades Source de traslación durante ese ciclo; no debe omitirse al reproducir caminata en mundo.
- Las pistas óseas tienen movimiento de compensación y deben mantenerse separadas del desplazamiento de entidad. No se debe eliminar un offset del hueso raíz arbitrariamente.

## Prueba visual más fiel (aún no reproducción bit-perfect de Valve)

Se exportaron **57 poses** consecutivas (índices 0 a 56) con el mismo `mdl+vvd+vtx`, sin IK postproceso, activando `--entity-motion` y renderizándolas en la misma cámara. El GIF reproduce frames 0..55 a 25 ms por muestra (40 FPS), para evitar duplicar el fotograma final del loop. La serie completa de 57 cuadros aparece en la hoja de capturas.

## Qué NO está implementado

- `CalcPoseSingle` completo: 1D/2D blending continuo con pose parameter y pesos por hueso.
- `CalcBoneQuaternion` de subframe: interpolación quaternion entre dos muestras RLE.
- `CalcAnimation` completo para secuencias por capas, autoplay y secciones externas.
- IK engine/autoría: el corpus `Walk_F` de fase 1 no declara reglas IK; el solver genérico no es su causa raíz.
- Orientación yaw de root motion cuando la pista contiene giro (el exportador OBJ solo aplica traslación); el motor sigue siendo dueño de la transformada de entidad.

## Instrucciones

```sh
make clean
make all CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
make check CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'

./hl2m_export_animated_obj89 model.mdl model.vvd model.dx90.vtx 1 28 frame028.obj --entity-motion
```

Los lectores y todos los vendors anteriores se conservan. La ABI master sigue en 0x0106 para no romper integraciones ya existentes (v1.0.7 corrige trigonometría y añade pruebas, sin cambiar firma de API).
