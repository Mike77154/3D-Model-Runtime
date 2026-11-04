#include "dff3d.h"

const char *dff3d_chunk_name(dff_u32 type)
{
    switch (type) {
    case DFF3D_SEC_STRUCT: return "Struct";
    case DFF3D_SEC_STRING: return "String";
    case DFF3D_SEC_EXTENSION: return "Extension";
    case DFF3D_SEC_TEXTURE: return "Texture";
    case DFF3D_SEC_MATERIAL: return "Material";
    case DFF3D_SEC_MATERIAL_LIST: return "MaterialList";
    case DFF3D_SEC_FRAME_LIST: return "FrameList";
    case DFF3D_SEC_GEOMETRY: return "Geometry";
    case DFF3D_SEC_CLUMP: return "Clump";
    case DFF3D_SEC_ATOMIC: return "Atomic";
    case DFF3D_SEC_TEXTURE_NATIVE: return "TextureNative";
    case DFF3D_SEC_TEXTURE_DICTIONARY: return "TextureDictionary";
    case DFF3D_SEC_GEOMETRY_LIST: return "GeometryList";
    case DFF3D_SEC_RIGHT_TO_RENDER: return "RightToRender";
    case DFF3D_SEC_UV_ANIM_DICT: return "UVAnimDictionary";
    case DFF3D_PLG_SKIN: return "SkinPLG";
    case DFF3D_PLG_HANIM: return "HAnimPLG";
    case DFF3D_PLG_BIN_MESH: return "BinMeshPLG";
    case DFF3D_PLG_NATIVE_DATA: return "NativeDataPLG";
    case DFF3D_PLG_MATFX: return "MaterialEffectsPLG";
    case DFF3D_PLG_UV_ANIM: return "UVAnimationPLG";
    case DFF3D_PLG_USER_DATA: return "UserDataPLG";
    case DFF3D_PLG_RSTAR_FRAME: return "RockstarFrame";
    case DFF3D_PLG_RSTAR_SPECULAR: return "RockstarSpecular";
    case DFF3D_PLG_RSTAR_REFLECTION: return "RockstarReflection";
    case DFF3D_PLG_RSTAR_2DFX: return "Rockstar2DFX";
    case DFF3D_PLG_RSTAR_NIGHT_COLORS: return "RockstarNightColors";
    case DFF3D_PLG_RSTAR_COLLISION: return "RockstarCollision";
    case DFF3D_PLG_SAMP_COLLISION: return "SAMPCollision";
    case DFF3D_PLG_SKYGFX: return "SkyGFX";
    default: return "Unknown";
    }
}

const char *dff3d_native_platform_name(dff_u32 platform)
{
    switch (platform) {
    case DFF3D_NATIVE_D3D7: return "D3D7";
    case DFF3D_NATIVE_OPENGL: return "OpenGL";
    case DFF3D_NATIVE_MAC: return "Mac";
    case DFF3D_NATIVE_PS2: return "PS2";
    case DFF3D_NATIVE_XBOX: return "Xbox";
    case DFF3D_NATIVE_GC: return "GameCube";
    case DFF3D_NATIVE_SOFT: return "SoftRaster";
    case DFF3D_NATIVE_D3D8: return "D3D8";
    case DFF3D_NATIVE_D3D9: return "D3D9";
    case DFF3D_NATIVE_PSP: return "PSP";
    case DFF3D_NATIVE_PS2_FOURCC: return "PS2FourCC";
    default: return "unknown";
    }
}

const char *dff3d_d3d_format_name(dff_u32 fmt)
{
    switch (fmt) {
    case 0x00000021UL: return "RGBA32";
    case 0x00000022UL: return "RGB32";
    case 0x31545844UL: return "DXT1";
    case 0x33545844UL: return "DXT3";
    case 0x35545844UL: return "DXT5";
    default: return "unknown";
    }
}
