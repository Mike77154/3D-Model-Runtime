# RE4 Classic Codec v5.2 — puente explícito SEQ → estado visual

**Estado:** Implementado y probado en C89. **No se han descubierto aún los
significados originales de los códigos SEQ.** No debe confundirse el ejemplo
de asignación de reglas con una tabla nativa de Resident Evil 4.

## Qué implementa

- `include/re4c3d/re4_seq_state.h`, `animation/re4_seq_state.c`: representación
  de estado sin heap (máscara de 32 piezas, estado de pose, 32 padres opcionales,
  reloj Q6) y conversión de eventos SEQ a acciones solamente **si el llamador
  proporciona una regla explícita**.
- Acciones: mostrar pieza (`show`), ocultar (`hide`), cambiar estado/pose (`pose`),
  asignar padre (`parent`), eliminar padre (`unparent`). Una asignación de padre
  es **solo un identificador de vínculo**: no resuelve aún matrices de anclaje.
- Las reglas pueden examinar el byte auxiliar de la clave o aceptar cualquier
  valor (`*`). Una regla general y una específica para el mismo código se
  rechazan por ambigüedad. Valor de código 0 **nunca** es una acción.
- `re4_seq_state_advance`: ventanas temporales contiguas, manejo de bucles,
  actualización **atómica** del estado. En modo estricto, cualquier código no
  reconocido devuelve `RE4_ERR_UNSUPPORTED` sin modificar el estado original.
- `re4_seq_state_seek`: permite reconstruir el estado completo desde un estado
  inicial declarado, incluso cuando el usuario retrocede la animación.
- Internamente usa el despachador determinista v5.0; no infiere parejas FCV/SEQ.
  Se aconseja pasar FCV y SEQ previamente aprobados para su uso concreto.
- `examples/re4_seq_state_inspect.c`: CLI para analizar SEQ reales con reglas
  aportadas por el usuario o autor de un mod. Modo normal reporta códigos sin
  regla; `--strict` rechaza el primer código no reconocido.

### Ejemplo de prueba, NO un significado del juego

```sh
./re4_seq_state_inspect clip.fcv eventos.seq 19072 \
    --rule=5:*:show:3:0
```

Esto **hipotéticamente** hace visible la pieza 3 cuando un SEQ emite el código 5.
No afirmamos que `5` signifique «mostrar pieza» en RE4. Para una asignación
real deben hallarse y verificarse los manejadores del ejecutable o la tabla
explícita del mod. La CLI solo verifica compatibilidad de duración FCV/SEQ;
no certifica que los dos recursos formen la pareja empleada por el juego.

## Pruebas verificadas

- 33 suites C89 con `-std=c89 -Wall -Wextra -pedantic`.
- Las mismas 33 suites con AddressSanitizer/UndefinedBehaviorSanitizer.
- Pruebas sintéticas: dos ciclos, sucesos simultáneos, byte auxiliar, cambio de
  padre y pose, regreso en el tiempo, rechazo de reglas ambiguas, rechazo de
  eventos sin regla **sin efectos parciales** y pose con ID 255 (sin desplazar
  una máscara por cantidad indefinida).
- Pruebas con DAT reales (`em.dat`): cuervo `em23` FCV7/SEQ24 (SEQ de 19 claves
  con todos los códigos a cero); Plaga Araña `em25` FCV15/SEQ16; Méndez `em35`
  FCV19/SEQ20; armadura `em3c` FCV25/SEQ26. Se reproducen dos ciclos. Las
  reglas ilustrativas **no son reglas nativas**.
- Barrido amplio: **3,403/3,403 parejas con coincidencia temporal única**,
  **148 familias de recursos**, **75,949 claves no nulas por vuelta total**;
  contadores coinciden entre C89 y lectura independiente de las bytes originales.
  Todos los códigos siguen sin interpretar por defecto.
- `evd.dat` original: 159 entradas, 156 con estructura de paquetes aceptada en
  el barrido externo. No aparecen `SetParts`, `SetEm`, `Mot` ni `ParentOn/Off` en
  las 156 entradas analizadas. Tres EVD tienen regiones de paquetes fuera del
  formato esperado y están pendientes; **no se declaran corruptos**.

## Límites y próximos pasos

1. Requiere análisis del ejecutable original o un mod de referencia para atribuir
   funciones verdaderas a los 50 valores de código SEQ observados anteriormente.
2. Las Plagas de cabeza A/B/C no disponen todavía de PMD y FCV vinculados
   inequívocamente. No reasignarles automáticamente la Plaga Araña `em25`.
3. Méndez fase 2: el desprendimiento del torso es parte de la forma del jefe,
   pero faltan estados de visibilidad y anclajes acreditados.
4. Parent IDs y máscara no sustituyen el cálculo de matrices world/bind; IK,
   eventos especiales, daño y Win32 siguen sin certificar.
5. El despachador actual limita los periodos a 65,536 unidades Q6 y a un número
   explícito de vueltas; clips más largos requieren otra implementación.

**Dependencias runtime:** únicamente código C89, memoria del llamador; sin
malloc/calloc/realloc/free, sin heap, sin float/double, sin long long.
No se redistribuyen PMD/FCV/SEQ/EVD ni texturas del juego.
