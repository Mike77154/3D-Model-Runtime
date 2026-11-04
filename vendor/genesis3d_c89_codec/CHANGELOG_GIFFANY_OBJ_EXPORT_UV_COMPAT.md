# Giffany OBJ Export UV Compatibility Patch

This patch changes the `g3dtool skin` / `skinblend` OBJ writer so exported `vt` records use the Genesis3D/fragMOTION-compatible negative V convention.

## Why

The reference OBJ produced by fragMOTION for `zombie.act` uses UVs like:

```obj
vt 0.4219 -0.9653
```

The previous codec exporter emitted positive V values from BODY xskin vertices. For preview tools that sample OBJ UVs directly, this makes the atlas appear vertically misinterpreted and harder to compare against external Genesis3D tool output.

## Change

Exporter now writes:

```text
vt.u = xskin.u
vt.v = -xskin.v
```

No heap, malloc, realloc, free, float/double scalar fields, or runtime allocations were added.
