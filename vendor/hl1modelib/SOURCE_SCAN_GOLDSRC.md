# Source scan GoldSrc

Se revisaron los zips subidos para orientar la mejora:

- `halflife_md_opener.zip`
  - La rama más completa era `hlfp_semantic_reemit`.
  - Ya tenía parser MDL/SMD, reemisión semántica, quats y controllers.

- `HalfLifeModelViewer2-master.zip`
  - Confirmó flujo de companion files:
    - main `.mdl`
    - texturas separadas como `baseT.mdl` cuando el main no trae texturas
    - sequence groups como `base01.mdl`, `base02.mdl`, etc.
    - variante `.dol` para Dreamcast en ese viewer.
  - Confirmó fórmula práctica de bodygroups:
    - `choice = (body / bodypart.base) % bodypart.nummodels`
    - reemplazo: `body = body - current * base + new_choice * base`

- `reGS_hlsdk-master.zip` y `GameEngine-reGoldSrc-main.zip`
  - Confirmaron la misma lógica de bodygroup en código de gameplay/animación.
  - Sirvieron para cruzar expectativas de skeleton, hitboxes, attachments, sequence data y controllers.

- `xash3d-fwgs-master.zip`
  - Señaló las extensiones modernas a vigilar después:
    - `studiohdr2`
    - flags extra de material
    - boneinfo/procedural/boneweights
    - seq/IK/autolayer extendidos
  - Esta entrega no copia esas estructuras al 100%; las deja como siguiente mordida.

- `HalfLifeAssetManager-master.zip`
  - Sirvió como referencia de flujo de edición/asset management: textura, UI/editor, inspección y guardado.

## Nota de licencia

No se copió código de esos proyectos dentro de `hlfp_goldsrc.c`. La implementación nueva es propia y se limitó a reimplementar comportamiento de formato/engine necesario para mantener tu estilo C89/no-heap/fixed-point.
