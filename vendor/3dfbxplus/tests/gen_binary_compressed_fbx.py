#!/usr/bin/env python3
import struct
import zlib
from pathlib import Path

MAGIC = b"Kaydara FBX Binary  \x00\x1a\x00"

def null_record(version: int) -> bytes:
    return b"\x00" * (25 if version >= 7500 else 13)

def prop_i64(v: int) -> bytes:
    return b"L" + struct.pack("<q", v)

def prop_str(s: str) -> bytes:
    b = s.encode("utf-8")
    return b"S" + struct.pack("<I", len(b)) + b

def prop_arr_i32(vals, compressed=True) -> bytes:
    raw = struct.pack("<" + "i" * len(vals), *vals)
    if compressed:
        payload = zlib.compress(raw, 6)
        return b"i" + struct.pack("<III", len(vals), 1, len(payload)) + payload
    return b"i" + struct.pack("<III", len(vals), 0, len(raw)) + raw

def prop_arr_f64(vals, compressed=True) -> bytes:
    raw = struct.pack("<" + "d" * len(vals), *vals)
    if compressed:
        payload = zlib.compress(raw, 6)
        return b"d" + struct.pack("<III", len(vals), 1, len(payload)) + payload
    return b"d" + struct.pack("<III", len(vals), 0, len(raw)) + raw

def build_node(name: str, props, children, start: int, version: int) -> bytes:
    name_b = name.encode("utf-8")
    prop_b = b"".join(props)
    header_size = 25 if version >= 7500 else 13
    cur = start + header_size + len(name_b) + len(prop_b)
    child_b = b""
    for ch in children:
        b = build_node(ch[0], ch[1], ch[2], cur, version)
        child_b += b
        cur += len(b)
    sentinel = null_record(version) if children else b""
    cur += len(sentinel)
    if version >= 7500:
        header = struct.pack("<QQQB", cur, len(props), len(prop_b), len(name_b)) + name_b
    else:
        header = struct.pack("<IIIB", cur, len(props), len(prop_b), len(name_b)) + name_b
    return header + prop_b + child_b + sentinel

def make(version: int, compressed: bool = True) -> bytes:
    geom_id = 123456789
    model_id = 987654321
    vertices = [-1.0, 0.0, -1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 1.0, -1.0, 0.0, 1.0]
    pvi = [0, 1, 2, -4]
    normals = [0.0, 1.0, 0.0] * 4
    uv = [0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0]
    uvi = [0, 1, 2, 3]

    header_ext = ("FBXHeaderExtension", [], [
        ("FBXHeaderVersion", [prop_i64(1003)], []),
        ("FBXVersion", [prop_i64(version)], []),
        ("Creator", [prop_str("3dfbxplus binary compressed generated corpus")], []),
    ])
    objects = ("Objects", [], [
        ("Geometry", [prop_i64(geom_id), prop_str("Geometry::Quad"), prop_str("Mesh")], [
            ("Vertices", [prop_arr_f64(vertices, compressed)], []),
            ("PolygonVertexIndex", [prop_arr_i32(pvi, compressed)], []),
            ("LayerElementNormal", [prop_i64(0)], [
                ("MappingInformationType", [prop_str("ByPolygonVertex")], []),
                ("ReferenceInformationType", [prop_str("Direct")], []),
                ("Normals", [prop_arr_f64(normals, compressed)], []),
            ]),
            ("LayerElementUV", [prop_i64(0)], [
                ("Name", [prop_str("UVMap")], []),
                ("MappingInformationType", [prop_str("ByPolygonVertex")], []),
                ("ReferenceInformationType", [prop_str("IndexToDirect")], []),
                ("UV", [prop_arr_f64(uv, compressed)], []),
                ("UVIndex", [prop_arr_i32(uvi, compressed)], []),
            ]),
        ]),
        ("Model", [prop_i64(model_id), prop_str("Model::Quad"), prop_str("Mesh")], []),
    ])
    conns = ("Connections", [], [
        ("C", [prop_str("OO"), prop_i64(geom_id), prop_i64(model_id)], []),
    ])

    body = b""
    cur = len(MAGIC) + 4
    for top in [header_ext, objects, conns]:
        b = build_node(top[0], top[1], top[2], cur, version)
        body += b
        cur += len(b)
    return MAGIC + struct.pack("<I", version) + body + null_record(version)

if __name__ == "__main__":
    out_dir = Path("tests/generated")
    out_dir.mkdir(parents=True, exist_ok=True)
    for ver in (7400, 7500, 7700):
        path = out_dir / f"binary_compressed_{ver}.fbx"
        path.write_bytes(make(ver, True))
        print(path, path.stat().st_size)
