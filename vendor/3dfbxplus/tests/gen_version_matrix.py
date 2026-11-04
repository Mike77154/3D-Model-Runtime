#!/usr/bin/env python3
"""Generate a broader deterministic FBX version battery.

The files are tiny by design: one textured/colorable quad with normals/UVs.
They probe parser differences by file version and container encoding:
- ASCII tree syntax
- binary raw arrays
- binary zlib-compressed arrays
"""
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "version_matrix"
OUT.mkdir(parents=True, exist_ok=True)

VERSIONS = [
    ("fbx5", 5000),
    ("fbx6", 6000),
    ("2010", 6100),
    ("fbx7", 7000),
    ("2011", 7100),
    ("2012", 7200),
    ("2013", 7300),
    ("2014", 7400),
    ("2015", 7400),
    ("2016", 7500),
    ("2016x", 7600),
    ("modern", 7700),
]

ASCII = '''; deterministic 3dfbx version matrix fixture
FBXHeaderExtension:  {{
    FBXHeaderVersion: 1003
    FBXVersion: {version}
    Creator: "3dfbx version matrix {label}"
}}
GlobalSettings: {{
    Version: 1000
}}
Objects:  {{
    Geometry: 123456789, "Geometry::Quad_{label}", "Mesh" {{
        Vertices: *12 {{
            a: -1,0,-1, 1,0,-1, 1,0,1, -1,0,1
        }}
        PolygonVertexIndex: *6 {{
            a: 0,1,2,-3,0,2,3,-4
        }}
        LayerElementNormal: 0 {{
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            Normals: *18 {{
                a: 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0
            }}
        }}
        LayerElementUV: 0 {{
            Name: "UVMap"
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            UV: *12 {{
                a: 0,0, 1,0, 1,1, 0,0, 1,1, 0,1
            }}
        }}
        LayerElementColor: 0 {{
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            Colors: *24 {{
                a: 1,0,0,1, 0,1,0,1, 0,0,1,1, 1,0,0,1, 0,0,1,1, 1,1,1,1
            }}
        }}
    }}
    Model: 987654321, "Model::Quad_{label}", "Mesh" {{
        Properties70:  {{
            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
        }}
    }}
    Material: 777, "Material::Mat_{label}", "" {{
        ShadingModel: "phong"
        Properties70: {{
            P: "DiffuseColor", "Color", "", "A",1,1,1
        }}
    }}
    Texture: 778, "Texture::Diffuse_{label}", "" {{
        Type: "TextureVideoClip"
        FileName: "textures/{label}.png"
        RelativeFilename: "textures/{label}.png"
    }}
}}
Connections:  {{
    C: "OO",123456789,987654321
    C: "OO",777,987654321
    C: "OP",778,777,"DiffuseColor"
}}
'''

MAGIC = b"Kaydara FBX Binary  \x00\x1a\x00"

def null_record(version: int) -> bytes:
    return b"\x00" * (25 if version >= 7500 else 13)

def prop_i64(v: int) -> bytes:
    return b"L" + struct.pack("<q", v)

def prop_str(s: str) -> bytes:
    b = s.encode("utf-8")
    return b"S" + struct.pack("<I", len(b)) + b

def prop_arr_i32(vals, compressed: bool) -> bytes:
    raw = struct.pack("<" + "i" * len(vals), *vals)
    if compressed:
        payload = zlib.compress(raw, 6)
        return b"i" + struct.pack("<III", len(vals), 1, len(payload)) + payload
    return b"i" + struct.pack("<III", len(vals), 0, len(raw)) + raw

def prop_arr_f64(vals, compressed: bool) -> bytes:
    raw = struct.pack("<" + "d" * len(vals), *vals)
    if compressed:
        payload = zlib.compress(raw, 6)
        return b"d" + struct.pack("<III", len(vals), 1, len(payload)) + payload
    return b"d" + struct.pack("<III", len(vals), 0, len(raw)) + raw

def node(name: str, props, children, start: int, version: int) -> bytes:
    nb = name.encode("utf-8")
    pb = b"".join(props)
    header_size = 25 if version >= 7500 else 13
    cur = start + header_size + len(nb) + len(pb)
    cb = b""
    for ch in children:
        b = node(ch[0], ch[1], ch[2], cur, version)
        cb += b
        cur += len(b)
    sentinel = null_record(version) if children else b""
    cur += len(sentinel)
    if version >= 7500:
        h = struct.pack("<QQQB", cur, len(props), len(pb), len(nb)) + nb
    else:
        h = struct.pack("<IIIB", cur, len(props), len(pb), len(nb)) + nb
    return h + pb + cb + sentinel

def binary_fixture(label: str, version: int, compressed: bool) -> bytes:
    vertices = [-1.0, 0.0, -1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 1.0, -1.0, 0.0, 1.0]
    pvi = [0, 1, 2, -3, 0, 2, 3, -4]
    normals = [0.0, 1.0, 0.0] * 6
    uv = [0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0]
    colors = [1.0, 1.0, 1.0, 1.0] * 6
    top = [
        ("FBXHeaderExtension", [], [
            ("FBXHeaderVersion", [prop_i64(1003)], []),
            ("FBXVersion", [prop_i64(version)], []),
            ("Creator", [prop_str(f"3dfbx version matrix {label}")], []),
        ]),
        ("Objects", [], [
            ("Geometry", [prop_i64(123456789), prop_str(f"Geometry::Quad_{label}"), prop_str("Mesh")], [
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
                    ("ReferenceInformationType", [prop_str("Direct")], []),
                    ("UV", [prop_arr_f64(uv, compressed)], []),
                ]),
                ("LayerElementColor", [prop_i64(0)], [
                    ("MappingInformationType", [prop_str("ByPolygonVertex")], []),
                    ("ReferenceInformationType", [prop_str("Direct")], []),
                    ("Colors", [prop_arr_f64(colors, compressed)], []),
                ]),
            ]),
            ("Model", [prop_i64(987654321), prop_str(f"Model::Quad_{label}"), prop_str("Mesh")], []),
            ("Material", [prop_i64(777), prop_str(f"Material::Mat_{label}"), prop_str("")], [
                ("ShadingModel", [prop_str("phong")], []),
            ]),
            ("Texture", [prop_i64(778), prop_str(f"Texture::Diffuse_{label}"), prop_str("")], [
                ("Type", [prop_str("TextureVideoClip")], []),
                ("FileName", [prop_str(f"textures/{label}.png")], []),
                ("RelativeFilename", [prop_str(f"textures/{label}.png")], []),
            ]),
        ]),
        ("Connections", [], [
            ("C", [prop_str("OO"), prop_i64(123456789), prop_i64(987654321)], []),
            ("C", [prop_str("OO"), prop_i64(777), prop_i64(987654321)], []),
            ("C", [prop_str("OP"), prop_i64(778), prop_i64(777), prop_str("DiffuseColor")], []),
        ]),
    ]
    body = b""
    cur = len(MAGIC) + 4
    for t in top:
        b = node(t[0], t[1], t[2], cur, version)
        body += b
        cur += len(b)
    return MAGIC + struct.pack("<I", version) + body + null_record(version)

for label, version in VERSIONS:
    base = f"quad_{label}_{version}"
    (OUT / f"{base}_ascii.fbx").write_text(ASCII.format(label=label, version=version))
    (OUT / f"{base}_binary_raw.fbx").write_bytes(binary_fixture(label, version, False))
    (OUT / f"{base}_binary_zlib.fbx").write_bytes(binary_fixture(label, version, True))

print(f"wrote {len(list(OUT.glob('*.fbx')))} fixtures to {OUT}")
