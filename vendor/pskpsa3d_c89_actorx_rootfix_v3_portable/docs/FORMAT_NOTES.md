# ActorX PSK/PSA notes implemented here

## Shared chunk header

```text
char  ChunkID[20]
u32   TypeFlag
u32   DataSize
u32   DataCount
bytes Data[DataSize * DataCount]
```

## Classic PSK chunk order

```text
ACTRHEAD
PNTS0000
VTXW0000
FACE0000
MATT0000
REFSKELT
RAWWEIGHTS
```

## Classic PSA chunk order

```text
ANIMHEAD
BONENAMES
ANIMINFO
ANIMKEYS
```

Some newer exporters may add extra chunks such as scale keys, vertex colors, normals, or extra UVs. This codec records and skips unknown chunks.

## Implemented record layouts

```text
VPoint:
  f32 x, y, z

VVertex classic, 16 bytes:
  u16 PointIndex
  u16 Pad
  f32 U
  f32 V
  u8  MatIndex
  u8  Reserved
  u16 Pad2

VTriangle classic, 12 bytes:
  u16 Wedge0, Wedge1, Wedge2
  u8  MatIndex
  u8  AuxMatIndex
  u32 SmoothingGroups

VMaterial, 88 bytes:
  char Name[64]
  u32 TextureIndex
  u32 PolyFlags
  u32 AuxMaterial
  u32 AuxFlags
  i32 LodBias
  i32 LodStyle

VBone / FNamedBoneBinary classic, 120 bytes:
  char Name[64]
  u32 Flags
  i32 NumChildren
  i32 ParentIndex
  f32 QuatX, QuatY, QuatZ, QuatW
  f32 PosX, PosY, PosZ
  f32 Length
  f32 SizeX, SizeY, SizeZ

VRawBoneInfluence, 12 bytes:
  f32 Weight
  u32 PointIndex
  u32 BoneIndex

AnimInfoBinary, 168 bytes:
  char Name[64]
  char Group[64]
  i32 TotalBones
  i32 RootInclude
  i32 KeyCompressionStyle
  i32 KeyQuotum
  f32 KeyReduction
  f32 TrackTime
  f32 AnimRate
  i32 StartBone
  i32 FirstRawFrame
  i32 NumRawFrames

VQuatAnimKey, 32 bytes:
  f32 PosX, PosY, PosZ
  f32 QuatX, QuatY, QuatZ, QuatW
  f32 Time
```
