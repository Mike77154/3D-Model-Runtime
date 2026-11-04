@echo off
rem Windows i686 C89 build without compressed-input support.
rem Full zlib/Zstandard Win32 binary/source vendor is NOT provided yet.
gcc -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude ^
src\main.c src\bc_binary.c src\bc_wrapper_stub.c src\bc_dna.c ^
src\bc_fixed.c src\bc_refs.c src\bc_uv.c src\bc_uv_tface.c ^
src\bc_mesh_legacy.c src\bc_mesh_modern.c src\bc_skin.c ^
src\bc_armature.c src\bc_anim_common.c src\bc_anim_fcurve.c ^
src\bc_anim_eval.c src\bc_anim_legacy.c src\bc_anim_layered.c ^
src\bc_anim_export.c src\bc_anim_ipo.c src\bc_pose.c ^
src\bc_rig_q16.c src\bc_euler_q16.c src\bc_object_links.c src\bc_anim_bindings.c src\bc_skin_tsv.c src\bc_playback_tsv.c ^
src\bc_rest_matrices.c src\bc_constraints.c src\bc_ik2.c src\bc_ik_chain.c src\bc_ik_audit.c -o blendc89.exe
if errorlevel 1 exit /b 1
echo Built blendc89.exe (uncompressed BLEND files only).
