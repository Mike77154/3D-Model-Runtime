# Format notes

## Modelos

### `.mesh.ascii` / `.ascii`

Orden práctico:

1. `bone_count`
2. huesos
3. `mesh_count`
4. meshes
   - nombre
   - `uv_layer_count`
   - `texture_count`
   - texturas
   - `vertex_count`
   - vértices
   - `face_count`
   - triángulos

### `.mesh`

Modelo binario sin header XPS.

### `.xps`

Mismo cuerpo general de `.mesh`, pero con header al inicio.

Header práctico:

1. `magic` = `323232` little-endian
2. `version_major`, `version_minor`
3. string `XNAaraL`
4. `settings_len_words`
5. `machine`
6. `user`
7. `files`
8. bloque `settings`

Interpretación actual del bloque `settings`:

- `legacy_raw`: se conserva como bloque opaco
- `typed`: se decodifican
  - `hash`
  - `item_count`
  - opciones `type/count/info`
  - `type 0` = `none`
  - `type 1` = `default pose` (texto embebido, redondeado a múltiplos de 4 bytes)
  - `type 2` = `flags` (pares `u32 flag_id`, `u32 raw_value`)

## Poses `.pose`

Una línea por hueso:

```text
bone name: rx ry rz tx ty tz sx sy sz
```

También se acepta la forma corta:

```text
bone name: rx ry rz tx ty tz
```

En la forma corta la escala queda implícita como:

```text
1 1 1
```

## BoneDict

Una línea por mapeo:

```text
oldName;newName
```

El parser tolera líneas vacías y comentarios con `#`.

## Escenas `.scene`

Flujo práctico de lectura:

1. versión `u16.u16`
2. `item_count`
3. items
   - `item_name`
   - `item_path`
   - visibilidad
   - escala
   - pose por hueso
   - posición del item
   - accesorios primarios
   - accesorios secundarios
   - glow
4. cámara
5. 3 luces
6. postprocesado
7. fondo
8. sky dome
9. tamaño de ventana

## Detección

- extensión `.scene` => `.scene`
- `323232` little-endian al inicio => `.xps`
- texto ASCII con `:` en la primera línea significativa => `.pose`
- texto ASCII con `;` entre dos nombres => `BoneDict`
- texto ASCII legible sin eso => `.mesh.ascii`
- en otro caso => `.mesh`
