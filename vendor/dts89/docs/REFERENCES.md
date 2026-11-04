# References

These references are the practical source material behind the library design.

## Official/community Torque documentation

- Torque DTS format specification:
  - https://torquegameengines.github.io/T3D-Documentation/content/documentation/Artist%20Guide/Formats/dts_format.html
- Torque DSQ format specification:
  - https://torquegameengines.github.io/T3D-Documentation/content/documentation/Artist%20Guide/Formats/dsq_format.html
- DAE2DTS exporter notes:
  - https://torquegameengines.github.io/T3D-Documentation/content/documentation/Artist%20Guide/Exporters/dae2dts.html
- TSShape API reference:
  - https://reference.torque3d.org/coding/class/classtsshape/
- TSShapeConstructor reference:
  - https://reference.torque3d.org/scripting/class/classtsshapeconstructor/

## Engine source / API areas to keep checking

- `TSShape` / `tsShape.cpp`
- `TSShape` / `tsShape.h`
- `TSMesh` / `tsMesh.h`
- `TSMaterialList` / `tsMaterialList.cpp`
- `MaterialList` / `materialList.cpp`

## Notes

The encoder/decoder layout intentionally mirrors the engine's split-pool serialization model:

```text
32-bit pool
16-bit pool
8-bit pool
```

Torque material resolution is not fully self-contained inside raw DTS payloads. In common game content, final material behavior is still driven by script sidecars such as `materials.cs`, plus runtime mapping by material target names.

## Source-code anchors used for v3.3-giffany

- Torque3D `TSShape::write` / `TSShape::read` split-pool layout: `smVersion | exporter<<16`, memory buffer dwords, 32-bit pool, 16-bit pool, 8-bit pool, then sequences.
- Torque3D v29 encrypted-shape branch: detects/writes an XOR-obfuscated pool using the engine compression key; `dts89` detects this and refuses to decode as plaintext.
- Torque3D `TSMesh::assemble/disassemble`: mesh payload interpretation depends on DTS version; v27+ has packed vertex payloads, and v26+ differs from older 16-bit primitive/index paths.

See the public Torque3D source tree files:

- `Engine/source/ts/tsShape.cpp`
- `Engine/source/ts/tsShape.h`
- `Engine/source/ts/tsMesh.cpp`

