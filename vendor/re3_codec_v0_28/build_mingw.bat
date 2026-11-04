@echo off
setlocal
set CFLAGS=-std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude

if "%CC%"=="" set CC=gcc
if "%AR%"=="" set AR=ar

%CC% %CFLAGS% -c src\re3_codec.c -o re3_codec.o
if errorlevel 1 exit /b 1
%AR% rcs libre3_codec.a re3_codec.o
if errorlevel 1 exit /b 1

%CC% %CFLAGS% tools\re3scd_parts_probe.c libre3_codec.a -o re3scd_parts_probe.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_pc_rebase_audit.c libre3_codec.a -o re3emd_pc_rebase_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_pc_rebase.c libre3_codec.a -o test_pc_rebase.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_pc_morph_blend.c libre3_codec.a -o test_pc_morph_blend.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_weighted_morph_audit.c libre3_codec.a -o re3emd_weighted_morph_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_info.c libre3_codec.a -o re3emd_info.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_parts.c libre3_codec.a -o re3emd_parts.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_morph.c libre3_codec.a -o re3emd_morph.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_morph_audit.c libre3_codec.a -o re3emd_morph_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_viewer.c libre3_codec.a -o re3emd_viewer.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_preview.c libre3_codec.a -o re3emd_preview.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_reference_audit.c libre3_codec.a -o re3emd_reference_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_anim_preview.c libre3_codec.a -o re3emd_anim_preview.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_motion_audit.c libre3_codec.a -o re3emd_motion_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_fidelity_audit.c libre3_codec.a -o re3emd_fidelity_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_clip_catalog.c libre3_codec.a -o re3emd_clip_catalog.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_compare_clips.c libre3_codec.a -o re3emd_compare_clips.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_morph_space_audit.c libre3_codec.a -o re3emd_morph_space_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_morph_delta.c libre3_codec.a -o test_morph_delta.exe
if errorlevel 1 exit /b 1

%CC% %CFLAGS% tests\test_synthetic.c libre3_codec.a -o test_synthetic.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_damage_channels.c libre3_codec.a -o re3emd_damage_channels.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_damage_stress.c libre3_codec.a -o re3emd_damage_stress.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_damage_compare.c libre3_codec.a -o re3emd_damage_compare.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_damage_families.c libre3_codec.a -o test_damage_families.exe
if errorlevel 1 exit /b 1
test_damage_families.exe
if errorlevel 1 exit /b 1
test_synthetic.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_visual_state.c libre3_codec.a -o test_visual_state.exe
if errorlevel 1 exit /b 1

%CC% %CFLAGS% tests\test_parts_set.c libre3_codec.a -o test_parts_set.exe
if errorlevel 1 exit /b 1

test_synthetic.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_tim.c libre3_codec.a -o test_tim.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_key_resolution.c libre3_codec.a -o test_key_resolution.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_texture_binding.c libre3_codec.a -o test_texture_binding.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_animation.c libre3_codec.a -o test_animation.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_morph_retarget.c libre3_codec.a -o test_morph_retarget.exe
if errorlevel 1 exit /b 1

test_synthetic.exe
if errorlevel 1 exit /b 1
test_tim.exe
if errorlevel 1 exit /b 1
test_animation.exe
if errorlevel 1 exit /b 1
test_morph_retarget.exe
if errorlevel 1 exit /b 1
test_visual_state.exe
if errorlevel 1 exit /b 1
test_parts_set.exe
if errorlevel 1 exit /b 1
test_pc_rebase.exe
if errorlevel 1 exit /b 1

rem v0.24: PLD player/PLW weapon container reader + tools.
%CC% %CFLAGS% -c src\re3_packed.c -o re3_packed.o
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3packed_info.c re3_packed.o libre3_codec.a -o re3packed_info.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3packed_preview.c re3_packed.o libre3_codec.a -o re3packed_preview.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3packed_motion_audit.c re3_packed.o libre3_codec.a -o re3packed_motion_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_packed_header.c re3_packed.o libre3_codec.a -o test_packed_header.exe
if errorlevel 1 exit /b 1
test_packed_header.exe
if errorlevel 1 exit /b 1
rem v0.25: texture VRAM merge, guarded damage retarget and special audits.
%CC% %CFLAGS% tests\test_zombie_damage.c libre3_codec.a -o test_zombie_damage.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_vram_composite.c re3_packed.o libre3_codec.a -o test_vram_composite.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3emd_damage_audit.c libre3_codec.a -o re3emd_damage_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tools\re3packed_special_audit.c re3_packed.o libre3_codec.a -o re3packed_special_audit.exe
if errorlevel 1 exit /b 1
test_zombie_damage.exe
if errorlevel 1 exit /b 1
test_vram_composite.exe
if errorlevel 1 exit /b 1
rem v0.27: explicit damage rig contract.
%CC% %CFLAGS% tools\re3emd_damage_contract.c libre3_codec.a -o re3emd_damage_contract.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_damage_contract.c libre3_codec.a -o test_damage_contract.exe
if errorlevel 1 exit /b 1
test_damage_contract.exe
if errorlevel 1 exit /b 1
rem v0.28: RE3 PC node-slot prefix hypothesis, directly from sampler decompilation.
%CC% %CFLAGS% tools\re3emd_damage_prefix_audit.c libre3_codec.a -o re3emd_damage_prefix_audit.exe
if errorlevel 1 exit /b 1
%CC% %CFLAGS% tests\test_damage_prefix.c libre3_codec.a -o test_damage_prefix.exe
if errorlevel 1 exit /b 1
test_damage_prefix.exe
if errorlevel 1 exit /b 1
echo RE3_codec v0.28.0 build and synthetic tests OK
endlocal
