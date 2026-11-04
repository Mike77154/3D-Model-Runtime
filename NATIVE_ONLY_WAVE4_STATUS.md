# Modelo Runtime Experimental — Native-only Wave 4

## Descripción / arquitectura

Esta entrega es **ModelRuntime89 nativo**, no un ejecutable GACKT, no GACKT-LANG, no `.gmesh`, `.ganim`, `.gmat`, `.gscene` ni archivos cooked. Los proveedores leen datos originales directamente mediante sus propias bibliotecas vendorizadas y emiten callbacks de `modelrt_visit()`. Sus representaciones específicas están disponibles por funciones de acceso `modelrt_native_*` mientras el archivo permanece abierto.

- **35 proveedores en el registro:** 14 originales del runtime + 21 integraciones de los 65 vendors no duplicados.
- **6 nuevos proveedores en esta versión**, IDs 55-60.
- **44 vendors restantes:** carpetas fuente incluidas, *NO* registrados como proveedores, sin prometer importación nativa aún.
- **`MODELRT_MAX_PROVIDERS=96`**; no se modificaron los identificadores de los 14 iniciales.
- Build: `make clean && make -j6` usando GCC `-std=c89 -pedantic-errors -Wall -Wextra -O2`.

## Proveedores incorporados en esta versión

| ID | Códec | Alcance real |
|--:|---|---|
| 55 | ASE | triángulos y UV; UTF-16 no soportado; animación de transformación no visitada |
| 56 | MD3 | geometría y cuadros animados de vértices; prueba último fotograma |
| 57 | VRML 1 | lectura y geometría simple; prueba externa limitada (fixture sintético) |
| 58 | VRML 97 | geometría de escenas WRL reales |
| 59 | Ogre | XML `.mesh.xml` funcionando; binario `.mesh` aún falla validación |
| 60 | glTF 2 | `.glb` o `.gltf` con buffers BIN relativos; posiciones, UV, normales, triangulación; no animación/transformación de nodos en visitor |

Consulta `NATIVE_ONLY_WAVE4_CORPUS.csv` para los casos y conteos concretos. Los resultados son **visitas de geometría**, no pruebas de fidelidad visual, color de texturas, transformación global, skinning o animación completa.

## Limitaciones conocidas y próximos trabajos

1. **Ogre binario**: corregido el reconocimiento de cabecera (`0x1000` más versión terminada en salto de línea); las mallas binarias probadas aún fallan la validación interna. No se habilitó una ruta que saltara comprobaciones.
2. **glTF**: data-URI/embebidos y glTF 1.0 no admitidos por esta ruta. Jerarquías/escenas y datos de animación pueden consultarse en estado nativo, pero el visitor actual emite triángulos por primitiva *sin aplicar transformaciones globales ni skinning*.
3. **ASE**: documentos UTF-16 requieren transcoding adicional; animaciones del archivo no reproducidas.
4. **VRML 1**: falta corpus externo más variado para evaluar compatibilidad real.
5. **Antiguos 14 proveedores**: se comprobaron FBX, DirectX X y MilkShape representativos; no se ejecutó la matriz completa histórica en esta versión. La regresión original v19 busca un fixture `tiny.ms3d` ausente del paquete fuente anterior, por lo que no puede usarse directamente como PASS de suite completa.
6. Hay **44 vendors aún sin wrapper funcional**; no se introdujo un passthrough genérico ni se reportaron falsos positivos.
7. Presupuestos estáticos/fixed-point bajo C89: revisar archivos muy grandes, overflow y fuzzer antes de distribuir para entradas no confiables.

## Archivos relevantes

- `src/providers/provider_native_ase89.c`
- `src/providers/provider_native_md389.c`
- `src/providers/provider_native_vrml1_89.c`
- `src/providers/provider_native_vrml97_89.c`
- `src/providers/provider_native_ogre89.c`
- `src/providers/provider_native_gltf89.c`
- `include/modelrt_native_gackt89.h` (nombre de compatibilidad histórica; sin GACKT-LANG)
- `EXPERIMENTAL_PROVIDER_STATUS.csv` (65 vendors y estado)
- `PENDING_VENDOR_PORTS_44.csv` (pendientes)
- `tests/native_wave4_regression.c` (harness enlazado contra la biblioteca principal; sin GACKT)

### Compilar y probar

```sh
make clean
make -j6
make native-wave4-test
./tests/native_wave4_regression /ruta/al/modelo.gltf /ruta/al/modelo.md3
```

No incluimos modelos externos del corpus en el ZIP de código fuente. Hay que pasar rutas a modelos locales para ejecutar las regresiones.
