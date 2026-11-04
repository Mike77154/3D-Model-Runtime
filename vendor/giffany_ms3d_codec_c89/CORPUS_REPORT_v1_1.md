# Giffany MS3D Codec C89 v1.1 — prueba real del corpus

**Fecha:** 2026-10-10  
**Corpus suministrado:** `night_of_the_zombies.lha`  
**Formato del modelo:** MilkShape 3D (`MS3D000000`, versión 4)

## Inventario del LHA

- **980 archivos** en el contenedor LHA, incluyendo audio, texturas, modelos 3D y metadatos.
- **157 archivos `.ms3d`**, de los cuales **156 tienen contenido binario distinto** (la copia de `farm.ms3d` es idéntica al original).
- La lectura secuencial inicial de libarchive se detuvo en el archivo n.º 114. Se pudo recorrer la estructura LHA nivel 0 con índices de posición y extraer cada modelo como una entrada individual. Un nombre con bytes de codificación no UTF-8 se normalizó únicamente en el encabezado temporal; el contenido del `.ms3d` se conservó.

## Prueba de códec

| Prueba | Antes | Ahora |
| --- | ---: | ---: |
| Modelos importados | 24/157 | **157/157** |
| Modelos con probe de índices/huesos/keyframes | No disponible | **157/157** |
| Animaciones/joints existentes en el corpus | 6 | **6** |

**Problema corregido:** la versión original retrocedía después de un bloque de comentarios opcionales válido con cuatro listas vacías, desalineando la lectura del bloque de pesos de huesos. También interpretaba `weights[0]` como peso residual en lugar de peso del hueso base.

## Límites máximos observados

| Campo | Máximo real | Modelo representativo |
| --- | ---: | --- |
| Vértices | 10,784 | `data/weapons/10/model.ms3d` |
| Triángulos | 11,418 | `data/weapons/6/model.ms3d` |
| Joints | 46 | `data/grenade_hud.ms3d` |
| Keyframes de rotación | 4,992 | `data/zomb.ms3d` |
| Keyframes de posición | 4,992 | `data/zomb.ms3d` |

## Modelos con esqueleto

| Modelo | Joints | Rot keys | Pos keys |
| --- | ---: | ---: | ---: |
| `data/grenade_hud.ms3d` | 46 | 1,702 | 1,702 |
| `data/kamikaze.ms3d` | 32 | 3,872 | 3,872 |
| `data/mlyn_anim2.ms3d` | 3 | 18 | 18 |
| `data/zmb.ms3d` | 19 | 4,769 | 4,769 |
| `data/zmb_hs.ms3d` | 19 | 4,769 | 4,769 |
| `data/zomb.ms3d` | 32 | 4,992 | 4,992 |

## Cobertura confirmada

- Vertices e indices de triangulos y de grupos dentro de sus limites.
- Atributos UV/normales por esquina, material/grupos por asset.
- Bones y pesos activos dentro de indices validos.
- Parent indices/jerarquias recorribles, sin ciclos en los modelos del corpus.
- Keyframes de rotacion/traslacion accesibles mediante sampling local.
- Exportacion OBJ/MTL en pose base para `zomb`, `kamikaze` y `auto`.

## Límites pendientes

Esto **no** demuestra reconstruccion de matrices globales, skinning deformable final, clips de animacion en el renderizador ni shaders originales. OBJ no almacena skeleton ni clips; el importer de C89 si conserva esos datos. Las vistas PNG aplican color de textura por triangulo (aproximacion), no rasterizacion UV completa.

## Reproduccion

```sh
make check
python3 tools/bench_corpus.py /ruta/a/157_modelos
./examples/ms3d_obj_export /ruta/a/zomb.ms3d zomb.obj
```

No se modifico el archivo LHA original, ni se incluyo el ejecutable original del videojuego en los paquetes.
