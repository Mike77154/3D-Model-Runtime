Se obtuvo `Sphere_DiffPinkBlueSpec_Alpha90.mdl`, de 3,720 bytes, del repositorio público
`assimp/assimp` (test/models/MDL/MDL7 (3DGS A7)). Se comprobó con el binario de nuestro
parser compilado localmente, sin Assimp enlazado:

- MDL7, 1 grupo, 0 huesos, 62 vértices, 120 triángulos, 0 UV, 0 frames.
- Skin sin imagen raster, material explícito: Kd=0.9255 0.0000 1.0000;
  opacidad d=0.9000 (conversión Q16.16).
- OBJ/MTL generados y **sin TGA vacía**, corrigiendo el fallo de Fase 04.
- Hash SHA-256 del archivo real: `9755c55e41fd9b41ddbada658a4c11ff26112c1d4103528d162d03586b1e60ab`.

**El archivo de terceros NO está redistribuido en el ZIP**. Se puede obtener del proyecto de
origen, de acuerdo con sus condiciones de uso. Los fixtures incluidos sí son originales.

Esta muestra real **no posee deformadores ni huesos**. Por tanto, NO certifica pesos de MDL7,
ni confirma la colocación exacta del bloque de deformadores en subversiones reales.
El modelo de pesos sigue siendo experimental y solo se valida con nuestros fixtures sintéticos.
