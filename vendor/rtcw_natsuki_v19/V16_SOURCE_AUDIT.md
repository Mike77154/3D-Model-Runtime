# Auditoría v0.16

Referencia de comportamiento consultada: `RTCW-SP-master.zip`, archivo `src/renderer/tr_shader.c`, para denominaciones de `blendFunc` y `alphaFunc`. Las implementaciones nuevas (`rtcw_shaderstyle.c`, `rtcw_transorder.c`) son independientes y CC0, sin código de motor externo incorporado. Los modelos reales no se distribuyen.

Se actualizó el sistema de renderizado original del proyecto para usar una cola global de triángulos translúcidos: sólo el pase opaco escribe profundidad. El orden de la cola es determinista y se basa en una métrica de cámara simplificada, no en las prioridades completas de las etapas Quake3. El parser de la primera etapa y su diagnosticador conservan la distinción entre lo implementado y lo no soportado. En mapas VFS, la ruta opcional del archivo `.shader` de referencia se conserva cuando existe una copia temporal utilizable; si no, se mantiene el comportamiento previo.

No se realizaron pruebas con assets comerciales originales ni con MSYS2 i686 nativo; también faltan efectos multietapa, prioridades por `sort` y los deformadores de shaders.
