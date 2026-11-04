@echo off
rem Execute from MSYS2 MINGW32 where gcc is i686-w64-mingw32-gcc.
cd /d "%~dp0"
gcc -O2 -std=c89 -pedantic -Wall -Wextra -Werror character_draw_bone_part.c hod2_binary.c hod2_corpus.c hod2_skin_audit.c hod2_skin_glb.c hod2_tables.c hod2_tablepack.c hod2_tablecsv.c hod2_pol.c hod2_tex.c hod2_mot.c hod2_glb.c hod2_cli.c ..\inherited\c89\hod2_resource_guard.c ..\inherited\c89\hod2_extra_parts.c ..\inherited\c89\hod2_character_clips.c -o hod2c89.exe
if errorlevel 1 exit /b 1
hod2c89.exe --help
