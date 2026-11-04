cmake_minimum_required(VERSION 3.10)
project(3dfbx C)

# C89/C90-ish. (We still rely on common compiler support for 64-bit ints.)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(DFBX_ENABLE_MINI_ZLIB "Enable internal mini-zlib compressed FBX array decoding" ON)
option(DFBX_BUILD_TESTS "Build small C smoke-test helpers" ON)
option(DFBX_BUILD_TOOLS "Build command-line tooling examples" ON)
option(DFBX_BUILD_GACKT_RUNTIME "Build runtime-only fixed GACKT reader" ON)

add_library(3dfbx STATIC
    3dfbx/fbx_alloc.c
    3dfbx/fbx_fixed.c
    3dfbx/fbx_gackt_export.c
    3dfbx/fbx_gackt_runtime.c
    3dfbx/special_adapters/fbx_legacy6000_adapter.c
    3dfbx/fbx_anim.c
    3dfbx/fbx_arrays.c
    3dfbx/fbx_connections.c
    3dfbx/fbx_importer.c
    3dfbx/fbx_importer_ascii.c
    3dfbx/fbx_importer_bin.c
    3dfbx/fbx_material.c
    3dfbx/fbx_mesh_ext.c
    3dfbx/fbx_nurbs.c
    3dfbx/fbx_scene.c
    3dfbx/fbx_scene_transverse.c
    3dfbx/fbx_version.c
    fbx_codec/fbx_mini_zlib.c
    fbx_codec/fbx_stream.c
    fbx_codec/fbx_semantic_writer.c
    fbx_codec/fbx_codec_decode.c
    fbx_codec/fbx_codec_writer_ascii.c
    fbx_codec/fbx_binary_writer.c
)

target_include_directories(3dfbx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/3dfbx
    ${CMAKE_CURRENT_SOURCE_DIR}/fbx_codec
)

if (NOT DFBX_ENABLE_MINI_ZLIB)
    target_compile_definitions(3dfbx PUBLIC FBX_NO_ZLIB=1)
endif()

if (DFBX_BUILD_TOOLS)
    add_executable(fbx_dump examples/fbx_dump.c)
    target_link_libraries(fbx_dump PRIVATE 3dfbx)

    add_executable(fbx_to_gackt examples/fbx_to_gackt.c)
    target_link_libraries(fbx_to_gackt PRIVATE 3dfbx)

    add_executable(fbx_to_fbxbin fbx_codec/tools/fbx_to_fbxbin.c)
    target_link_libraries(fbx_to_fbxbin PRIVATE 3dfbx)

    add_executable(fbx_to_fbxascii fbx_codec/tools/fbx_to_fbxascii.c)
    target_link_libraries(fbx_to_fbxascii PRIVATE 3dfbx)

    add_executable(fbx_to_obj examples/fbx_to_obj.c)
    target_link_libraries(fbx_to_obj PRIVATE 3dfbx)

    add_executable(fbx_preview_ppm fbx_codec/tools/fbx_preview_ppm.c)
    target_link_libraries(fbx_preview_ppm PRIVATE 3dfbx)

    add_executable(fbx_probe examples/fbx_probe.c)
    target_link_libraries(fbx_probe PRIVATE 3dfbx)
endif()

if (DFBX_BUILD_TESTS)
    add_executable(miniz_smoke tests/miniz_smoke.c)
    target_link_libraries(miniz_smoke PRIVATE 3dfbx)

    add_executable(morph_smoke tests/morph_smoke.c)
    target_link_libraries(morph_smoke PRIVATE 3dfbx)

    add_executable(stream_smoke tests/stream_smoke.c)
    target_link_libraries(stream_smoke PRIVATE 3dfbx)

    add_executable(stream_scan_smoke tests/stream_scan_smoke.c)
    target_link_libraries(stream_scan_smoke PRIVATE 3dfbx)

    add_executable(anim_eval_smoke tests/anim_eval_smoke.c)
    target_link_libraries(anim_eval_smoke PRIVATE 3dfbx)

    add_executable(semantic_smoke tests/semantic_smoke.c)
    target_link_libraries(semantic_smoke PRIVATE 3dfbx)

    add_executable(semantic_writer_smoke tests/semantic_writer_smoke.c)
    target_link_libraries(semantic_writer_smoke PRIVATE 3dfbx)

    add_executable(semantic_extended_smoke tests/semantic_extended_smoke.c)
    target_link_libraries(semantic_extended_smoke PRIVATE 3dfbx)

    add_executable(nurbs_smoke tests/nurbs_smoke.c)
    target_link_libraries(nurbs_smoke PRIVATE 3dfbx)
endif()

if (DFBX_BUILD_GACKT_RUNTIME)
    # Runtime-only fixed GACKT reader: no stdio, no zlib, no importer, no scene builder.
    add_library(gackt_runtime STATIC
        3dfbx/fbx_fixed.c
        3dfbx/fbx_gackt_runtime.c
    )
    target_compile_definitions(gackt_runtime PUBLIC FBX_RUNTIME_ONLY=1 FBX_NO_STDIO=1 FBX_NO_ZLIB=1)
    target_include_directories(gackt_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3dfbx)
endif()
