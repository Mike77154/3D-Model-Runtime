# RE1codec canonical byte stream v1

El stream canónico no es un formato de juego ni pretende reemplazar EMD. Es exclusivamente un contrato de prueba para que dos importadores puedan ser comparados con `memcmp`.

Orden little-endian:

```text
u32 magic = 'R1C1'
u32 version = 1

bank A semantic payload
bank B semantic payload
mesh semantic/raw payload
texture semantic/raw payload
```

Por bank se serializan metadata normalizada, pivotes en centi-units **más sus 6 bytes raw**, parent/children, clips, frame refs, la interpretación de cada pose y además **los `frame_size` bytes raw de cada pose**. Por mesh se serializan counts por objeto, `scale_word`, vertices raw + world-axis centi, normals raw y el packet de 28 bytes completo de cada primitive. Por texture se serializan metadata TIM, CLUT raw y pixel data raw.

Los offsets absolutos del archivo no participan en la identidad canónica. Por ello un importador que entregue las seis secciones por separado puede compararse byte a byte contra el importador del trailing directory.
