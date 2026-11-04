#!/usr/bin/env python3
"""Regenerate the tiny synthetic ASCII FBX smoke corpus.

These fixtures are intentionally small and deterministic. They are not a
replacement for real Blender/Maya/3ds Max/Mixamo corpora; they are regression
probes for the importer, scene builder, fixed conversion, materials, colors,
camera/light metadata, and version-profile diagnostics.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "generated"
OUT.mkdir(parents=True, exist_ok=True)

QUAD_TEMPLATE = '''; FBX generated regression fixture
FBXHeaderExtension:  {{
    FBXHeaderVersion: 1003
    FBXVersion: {version}
}}
Objects:  {{
    Geometry: 123456789, "Geometry::Quad", "Mesh" {{
        Vertices: *12 {{
            a: -1,0,-1, 1,0,-1, 1,0,1, -1,0,1
        }}
        PolygonVertexIndex: *4 {{
            a: 0,1,2,-4
        }}
        LayerElementNormal: 0 {{
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            Normals: *12 {{
                a: 0,1,0, 0,1,0, 0,1,0, 0,1,0
            }}
        }}
        LayerElementUV: 0 {{
            Name: "UVMap"
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "IndexToDirect"
            UV: *8 {{
                a: 0,0, 1,0, 1,1, 0,1
            }}
            UVIndex: *4 {{
                a: 0,1,2,3
            }}
        }}
    }}
    Model: 987654321, "Model::Quad", "Mesh" {{
        Properties70:  {{
            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
        }}
    }}
}}
Connections:  {{
    C: "OO",123456789,987654321
}}
'''

SIMPLE_TEMPLATE = '''; FBX 7.4.0 project file
FBXHeaderExtension:  {{
    FBXHeaderVersion: 1003
    FBXVersion: {version}
    Creator: "3dfbxplus generated corpus"
}}
GlobalSettings:  {{
    Version: 1000
}}
Objects:  {{
    Model: 1, "Model::Cube", "Mesh" {{
        Version: 232
        Properties70:  {{
            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
        }}
    }}
    Geometry: 2, "Geometry::CubeMesh", "Mesh" {{
        Vertices: *12 {{
            a: -1,-1,0, 1,-1,0, 1,1,0, -1,1,0
        }}
        PolygonVertexIndex: *6 {{
            a: 0,1,2, -3, 0,2,3,-4
        }}
        LayerElementNormal: 0 {{
            Version: 101
            Name: "Normals"
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            Normals: *18 {{
                a: 0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,1
            }}
        }}
        LayerElementUV: 0 {{
            Version: 101
            Name: "UVMap"
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            UV: *12 {{
                a: 0,0, 1,0, 1,1, 0,0, 1,1, 0,1
            }}
        }}
        LayerElementColor: 0 {{
            Version: 101
            Name: "Color"
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            Colors: *24 {{
                a: 1,0,0,1, 0,1,0,1, 0,0,1,1, 1,0,0,1, 0,0,1,1, 1,1,1,1
            }}
        }}
        Layer: 0 {{
            Version: 100
            LayerElement:  {{ Type: "LayerElementNormal" TypedIndex: 0 }}
            LayerElement:  {{ Type: "LayerElementUV" TypedIndex: 0 }}
            LayerElement:  {{ Type: "LayerElementColor" TypedIndex: 0 }}
        }}
    }}
    Material: 3, "Material::Mat_Red", "" {{
        Version: 102
        ShadingModel: "phong"
        Properties70:  {{
            P: "DiffuseColor", "Color", "", "A",0.8,0.1,0.1
            P: "SpecularColor", "Color", "", "A",0.2,0.2,0.2
            P: "EmissiveColor", "Color", "", "A",0,0,0
            P: "Opacity", "Number", "", "A",1
        }}
    }}
    Texture: 4, "Texture::DiffuseTex", "" {{
        Type: "TextureVideoClip"
        FileName: "textures/diffuse.png"
        RelativeFilename: "textures/diffuse.png"
    }}
}}
Connections:  {{
    C: "OO",2,1
    C: "OO",3,1
    C: "OP",4,3,"DiffuseColor"
}}
'''

CAM_LIGHT = '''; FBX generated camera/light smoke fixture
FBXHeaderExtension:  {
    FBXHeaderVersion: 1003
    FBXVersion: 7400
    Creator: "3dfbxplus generated corpus"
}
Objects:  {
    Model: 10, "Model::Camera", "Camera" {
        Version: 232
        Properties70:  {
            P: "ProjectionType", "enum", "", "A",1
            P: "FieldOfView", "Number", "", "A",45
            P: "NearPlane", "Number", "", "A",0.1
            P: "FarPlane", "Number", "", "A",1000
            P: "AspectW", "Number", "", "A",16
            P: "AspectH", "Number", "", "A",9
            P: "Lcl Translation", "Lcl Translation", "", "A",0,2,-5
            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
        }
    }
    Model: 20, "Model::KeyLight", "Light" {
        Version: 232
        Properties70:  {
            P: "LightType", "enum", "", "A",0
            P: "Intensity", "Number", "", "A",75
            P: "Color", "Color", "", "A",1,0.8,0.6
            P: "Cone angle", "Number", "", "A",30
            P: "DecayStart", "Number", "", "A",15
            P: "Lcl Translation", "Lcl Translation", "", "A",2,4,-3
            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
        }
    }
}
Connections:  {
}
'''

for version in [6000, 6100, 7000, 7100, 7200, 7300, 7400, 7500, 7600, 7700]:
    (OUT / f"quad_{version}_ascii.fbx").write_text(QUAD_TEMPLATE.format(version=version))
for version in [6100, 7000, 7100, 7200, 7300, 7400, 7500]:
    (OUT / f"simple_{version}_ascii.fbx").write_text(SIMPLE_TEMPLATE.format(version=version))
(OUT / "camera_light_7400_ascii.fbx").write_text(CAM_LIGHT)
print(f"wrote fixtures to {OUT}")
