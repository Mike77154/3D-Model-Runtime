# T-103 XNALara/XPS Smoke Test

Input model folder tested externally:
- `T-103 Mr. X/xps.xps`
- `T-103 Mr. X/Generic_Item.mesh`

Build command:

```sh
make clean
make CFLAGS='-std=c89 -pedantic -Wall -Wextra -Werror -O2'
```

Result:
- C89 strict build: PASS
- `xps.xps` parse: PASS
- `Generic_Item.mesh` parse: PASS
- No source hits for malloc/realloc/calloc/free/heap/float/double/stdin: PASS

Parsed geometry from `xps.xps`:
- Format: xps header v2.15
- Bones: 68
- Meshes: 4
- Vertices: 4674
- Faces: 7557
- Bounding box approx: X[-1.315292, 1.315292], Y[-0.002167, 2.317612], Z[-0.235046, 0.260284]

Parsed geometry from `Generic_Item.mesh`:
- Format: mesh
- Bones: 61
- Meshes: 4
- Vertices: 4673
- Faces: 7557

Added `examples/example_obj.c` and Makefile target `example_obj` for a tiny geometry smoke render/export path:

```sh
./example_obj path/to/xps.xps t103_from_xps.obj
```

The exporter is a test/example utility. It streams callbacks to OBJ and avoids malloc/realloc/free/float/double too.
