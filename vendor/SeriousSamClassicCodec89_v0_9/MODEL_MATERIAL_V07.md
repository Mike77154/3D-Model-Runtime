# Serious Sam Classic Codec89 v0.7 — Material y Kleer

## Alcance verificado

Se compararon dos implementaciones originales de Serious Engine 1.x proporcionadas por el usuario (`Serious-Engine-master.zip` y `SeriousSamClassic-main.zip`) en las rutas `Sources/Engine/Models/RenderModel_View.cpp`, `RenderModel.cpp` y `Model.cpp`. Esta implementación es independiente y no enlaza ni copia código GPL del motor.

**Archivos de prueba reales:** `SE1_10.gro` aportado por el usuario; se extraen fuera del ZIP de código. `Models/Enemies/Boneman/Boneman.mdl` + `Boneman.tex`, y los modelos/texturas de Headman, Scorpman y Werebull. En Serious Sam Classic, Boneman corresponde al Kleer Skeleton.

### Cambios de la v0.7

- `ss89normals.c`: decodificación independiente de las normales AV17 de 16 bits (heading/pitch) mediante LUTs de valores Q15 generados a partir de las ecuaciones trigonométricas. Sin punto flotante en tiempo de ejecución. Se interpolan vectores entre fotogramas y se normalizan con raíz entera.
- `ss89scene`: salida OBJ `vn` y caras `v/vt/vn` para modelos AV17, respetando rotación de attachment. Para vértices AFVX de 8 bits se preservan caras `v/vt` hasta disponer de un mapa completo del índice de normales Gouraud original.
- `ss89present`: modo directional que usa normales de vértice interpoladas cuando el material es `SST_MATTE`, modo FULLBRIGHT sin iluminación y alternativa legacy para comparar. `SST_FLAT` se sombrea por triángulo.
- Filtrado `--filter bilinear` (opcional) sobre atlas PPM y PAM, con UV periódicas y canal alfa. El filtrado es CPU, no reproduce aún las heurísticas de mip hardware.
- Transparencias: conserva las reglas `STT_OPAQUE`, `STT_TRANSLUCENT`, `STT_ADD`, `STT_TRANSPARENT`, `STT_MULTIPLY`; dibuja triángulos opacos primero y transparentes después, ordenados por profundidad media. El ordenado por triángulo no es equivalente al orden exacto de todas las superficies del motor original.
- `--alpha-cutoff 0..255` para inspeccionar transparencia por máscara. Los materiales con alpha-test siguen usando la opacidad del `.tex` auténtico.
- `--spec-map FILE.ppm` y `--reflection-map FILE.ppm` activan una **aproximación** de las respectivas capas solo en superficies con SRF_SPECULAR / SRF_REFLECTIONS. Son texturas optativas proporcionadas por el usuario, no un generador mágico de mapas nativos. La reflexión usa lookup de la normal en un mapa 2D y los colores de la superficie; no es el spherical mapping exacto de Croteam. Si no hay archivo ni flag, no se añade el efecto.
- `--materialize N` permite una prueba de *fade* del modelo de 0 a 255, semejante a la opacidad general de la entidad. **No reproduce** partículas de aparición del juego ni efectos de spawn nativos.
- `ss89scene --mip N` permite cambiar LOD y `--texture-frame N` permite escoger el frame de una `.tex` de imágenes crudas. No habilita efectos `FXDT`.
- Conserva v0.6: bump y detail (aproximaciones en modelos que los solicitan), haze/fog de demostración, sombra de contacto y marca de impacto de depuración.

### Resultado del Kleer original

```
Boneman.mdl:       538 vertices, 893 triangles (LOD0)
                   609 triangles (LOD2)
                   28 surfaces, 126 frames, 16 animation clips
RUNLOOP:           10 animation keys
Boneman.tex:       TVER4, 256x256 RGBA, 1 frame, MEX 2048x2048
Surface behavior:  alpha test y SRF_DOUBLESIDED en parte de mandíbula y torso
                   no SRF_BUMP / SRF_DETAIL / SRF_SPECULAR / SRF_REFLECTIONS
```

Las diez claves se exportaron como OBJ/MTL/FX89 y se renderizaron a un GIF desde `ss89present` en C89. Otros modelos comprobados: Headman (805 triángulos), Scorpman (1099 triángulos, 24 superficies SPECULAR), Werebull (732 triángulos). **Scorpman necesita el mapa especular correspondiente para recrear esa pasada auténtica**; su flag sin un mapa no autoriza reemplazarlo por un color inventado.

### Ejemplo MINGW32 (copia los assets desde tu propio GRO)

```
sh build_msys2_mingw32.sh
./bin/ss89scene.exe unused.amc --root D:/SeriousSamAssets \
  --model Models/Enemies/Boneman/Boneman.mdl \
  --texture Models/Enemies/Boneman/Boneman.tex \
  --root-clip RUNLOOP --key 3 --obj Kleer.obj
./bin/ss89present.exe Kleer.obj Kleer.ppm 145 --filter bilinear --lighting directional
```

Opciones útiles del presenter:

```
--filter nearest|bilinear
--lighting legacy|directional
--alpha-cutoff 0..255
--materialize 0..255
--spec-map FILE.ppm
--reflection-map FILE.ppm
--bump --aux-map FILE.ppm
--detail --aux-map FILE.ppm
--haze 0..255 --fog 0..255 --shadow 0..255 --decal
```

Opciones útiles del ensamblador: `--mip 0..n`, `--texture-frame N`, `--sequence PREFIX --count N --step-ms T`, `--time-ms T` y `--root-clip CLIP`.

### Limitaciones e interpretación honesta

No es todavía una réplica píxel por píxel de Serious Engine: falta el mapa de normales de 8 bits, selección automática de mip basada en distancia, filtros anisotrópicos, trilineal, gamma, ambiente y luces de escena, shadows proyectadas sobre el mundo, decals topológicos, lógica de efectos `FXDT`, mapa esférico de reflejos y especular original, renderizado de segunda cara por winding y mezcla estricta de las pasadas GPU. Las vistas C89 son pruebas funcionales, no capturas del motor original. Se conserva el protocolo C89 estricto, enteros de 32 bits, buffers estáticos, sin malloc/free y sin float/double.

### Pruebas ejecutadas

- `sh build.sh`: C89 estricto con `-pedantic-errors -Wall -Wextra -Werror`.
- `tests/run_tests.py`, `test_resources.py`, `test_skinning.py`, `test_amc_scene.py`, `test_present_fx.py`, `test_material_v07.py`, `bin/test_fx_api` y `bin/test_fxpasses`.
- `test_real_sam_sync_optional.py` con `SeriousSam.amc` real, cuatro piezas originales y sincronización de ciclos.
- Modelos originales: Kleer, Headman, Scorpman, Werebull. Prueba visual de 10 fotogramas de RUNLOOP. La prueba no mide igualdad con píxeles del ejecutable del juego.
