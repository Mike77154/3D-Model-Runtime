# SeriousSamClassicCodec89 v0.9 — reflexión y especular por modelo

## Alcance exacto

Esta revisión añade dos módulos C89 autocontenidos:

* `include/ss89settings.h` / `src/ss89settings.c`: reconocimiento de las referencias `FXTS`, `FXTR` y `FXTB` en el contenedor binario `WTEX` (archivos `.ini` de configuración del modelo). Las rutas `DFNM` tienen validación de longitud y traversal. `ss89settings` permite inspeccionarlas.
* `include/ss89env.h` / `src/ss89env.c`: construcción original en enteros de vectores de reflexión/especular y proyección de esfera con normales por vértice. No contiene código copiado del motor.

`ss89present` ahora acepta `--settings Modelo.ini --assets-root DIR`. Recupera los mapas `.tex` originales desde sus nombres en WTEX, los convierte temporalmente a PAM con `ss89_resource`, aplica las pasadas **sólo** si las superficies del `.fx89` habilitan reflexión o especular y elimina los intermedios temporales. Si el usuario da manualmente `--spec-map` o `--reflection-map`, esas rutas prevalecen.

## Correspondencia encontrada en Serious Engine 1.10

Fuente: `Sources/Engine/Models/RenderModel_View.cpp`, secciones REFLECTION y SPECULAR, después de las pasadas diffuse.

* Reflection: reflejar el vector al observador alrededor de las normales del modelo; mapa esfera con ejes X/Z y singularidad en -Y; mezcla `SRC_ALPHA, INV_SRC_ALPHA`.
* Specular: reflejar el vector luminoso alrededor de las normales y pasarlo a espacio de vista; mapa esfera con ejes X/Y y singularidad en -Z; suma `INV_SRC_ALPHA, ONE`.
* No se activa la pasada si el `ModelObject` no tiene textura cargada o si la superficie no habilita esa capa.
* La iluminación, las tintas modelo/superficie y los factores del nivel afectan el resultado final. `ss89present` interpreta las tintas de superficie del `.fx89`, pero **todavía NO reconstruye** las luces ambientales, la rotación mundial, los shaders GPU, color global del objeto, ni el estado de `ModelObject` en un nivel.
* Para mapas de especular RGB de prueba (sin alfa), el presenter usa cobertura plena. Los originales RGBA respetan alfa inversa como la pasada original. No afirmamos equivalencia píxel por píxel.

## Caso original comprobado

`Sources/Models/Enemies/Headman/Projectile/Blade.ini` contiene:

```
FXTS Models/SpecularTextures/Medium.tex
FXTR Models/ReflectionTextures/LightBlueMetal01.tex
```

En la distribución aportada, ambos `.tex` aparecen en `SE1_10.gro`.

Resultados de la prueba original reproducible (`tests/test_real_env_v09.py`):

* Blade.mdl: la imagen **cambia** cuando activamos los mapas solicitados por sus superficies.
* Boneman.mdl (Kleer): las imágenes son **idénticas byte a byte**, aun proporcionando mapas de Blade, porque sus materiales no activan esas capas.
* Siguen abriendo 390/390 `.mdl` originales con el inspector C89 de la v0.8.

## Compilación

```sh
sh build.sh                # Linux: compila C89 estricto
sh build_msys2_mingw32.sh # Windows MSYS2 MINGW32
```

## Ejemplo independiente (recursos previamente extraídos)

```sh
./bin/ss89settings Models/Enemies/Headman/Projectile/Blade.ini

./bin/ss89scene unused.amc --root Assets \
  --model Models/Enemies/Headman/Projectile/Blade.mdl \
  --texture Models/Enemies/Headman/Projectile/Blade.tex \
  --frame 0 --obj Blade.obj --gouraud-table ss89_gouraud.q15

./bin/ss89present Blade.obj Blade.ppm 145 \
  --filter bilinear \
  --settings Models/Enemies/Headman/Projectile/Blade.ini \
  --assets-root Assets
```

`--env-mode legacy` permite contrastar el algoritmo antiguo; `--env-mode sphere` es el valor nuevo por defecto.

Para validar directamente desde el ZIP original que entregó el usuario:

```sh
python3 tests/test_real_env_v09.py /ruta/Serious-Engine-master.zip
```

Este test extrae archivos a una carpeta temporal privada y los elimina al terminar. La biblioteca distribuida **no incluye recursos originales del juego, tablas de normales GPL ni fragmentos del código fuente del Serious Engine**.

## Limitaciones futuras

La reflexión real tiene dependencia de la cámara y las orientaciones objeto/mundo. Los efectos de iluminación dependen de la escena, color de luces, sombras y niebla. El rasterizador CPU usa iluminación direccional de demostración, y su interpolación y blending no son clones exactos de OpenGL/DirectX originales. La reconstrucción del *ModelObject* completo y su integración con iluminación de niveles queda pendiente.
