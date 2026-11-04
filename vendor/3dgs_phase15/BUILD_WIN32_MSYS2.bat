@echo off
REM Native strict C89; run from root with MinGW32 GCC in PATH
cd /d "%~dp0"
setlocal EnableDelayedExpansion
set SOURCES=
for /R src %%F in (*.c) do (
  echo %%F | findstr /I /C:"\tools\" >nul
  if errorlevel 1 set SOURCES=!SOURCES! "%%F"
)
set FLAGS=-std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude
if not exist bin mkdir bin
gcc %FLAGS% %SOURCES% src\tools\gs3d_cli.c -o bin\gs3d_probe.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% src\tools\gs3d_audit_main.c -o bin\gs3d_audit.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% src\tools\gs3d_export_main.c -o bin\gs3d_export.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_c89.c -o bin\gs3d_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_robustness.c -o bin\gs3d_robustness.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_pose.c -o bin\gs3d_pose_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_deform_weights.c -o bin\gs3d_deform_weights_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_material_lumps.c -o bin\gs3d_material_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase06.c -o bin\gs3d_phase06_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase06_robust.c -o bin\gs3d_phase06_robust.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase07.c -o bin\gs3d_phase07_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase07_robust.c -o bin\gs3d_phase07_robust.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% src\tools\gs3d_clip_main.c -o bin\gs3d_clip.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase08.c -o bin\gs3d_phase08_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase08_robust.c -o bin\gs3d_phase08_robust.exe
if errorlevel 1 exit /b 1
echo OK: modular 3DGS A4-A8 codec with deformers, weights, materials, and MED boundaries

gcc %FLAGS% %SOURCES% src\tools\gs3d_blend_main.c -o bin\gs3d_blend.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase09.c -o bin\gs3d_phase09_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase09_robust.c -o bin\gs3d_phase09_robust.exe
if errorlevel 1 exit /b 1

rem Phase10: module exports and tests (do not claim native Win32 certification).
gcc %FLAGS% %SOURCES% src\tools\gs3d_layer_main.c -o bin\gs3d_layer.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase10.c -o bin\gs3d_phase10_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase10_robust.c -o bin\gs3d_phase10_robust.exe
if errorlevel 1 exit /b 1

rem Phase11: additive pose and loop phase-sync.
gcc %FLAGS% %SOURCES% src\tools\gs3d_additive_main.c -o bin\gs3d_additive.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase11.c -o bin\gs3d_phase11_tests.exe
if errorlevel 1 exit /b 1
gcc %FLAGS% %SOURCES% tests\test_phase11_robust.c -o bin\gs3d_phase11_robust.exe
if errorlevel 1 exit /b 1
