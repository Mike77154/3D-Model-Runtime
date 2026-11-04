@echo off
setlocal
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_view.c -o rtcw_view.exe || exit /b 1
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_motion.c rtcw_anim_main.c -o rtcw_anim.exe || exit /b 1
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_motion.c rtcw_attach.c rtcw_attach_main.c -o rtcw_attach.exe || exit /b 1
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_mds_anim_main.c -o rtcw_mds_anim.exe || exit /b 1
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texview_main.c -o rtcw_texview.exe || exit /b 1
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_motion.c rtcw_attach.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c rtcw_multiattach_main.c -o rtcw_multiattach.exe || exit /b 1
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_pk3.c rtcw_pk3_main.c -o rtcw_pk3.exe || exit /b 1
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_pk3.c rtcw_vfs.c rtcw_family.c rtcw_clipcfg.c rtcw_family_main.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c -o rtcw_family.exe || exit /b 1
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_pk3.c rtcw_vfs.c rtcw_vfs_main.c -o rtcw_vfs.exe || exit /b 1
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_clipbank.c rtcw_clips_main.c rtcw_vfs.c rtcw_pk3.c -o rtcw_clips.exe || exit /b 1
rem El onceavo ejecutable se compila a continuacion.

rem V0.10 wolfanim.script structural graph
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_animscript.c rtcw_animscript_main.c rtcw_clipbank.c rtcw_vfs.c rtcw_pk3.c -o rtcw_script.exe
if errorlevel 1 exit /b 1
echo 11 ejecutables generados.

rem v0.11 explicit deterministic preview
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_motion.c rtcw_clipbank.c rtcw_animscript.c rtcw_preview.c rtcw_preview_main.c rtcw_vfs.c rtcw_pk3.c -o rtcw_preview.exe
if errorlevel 1 exit /b 1
echo 12 ejecutables generados.

rem v0.12 animated multipart rig + offline contact sheets
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_motion.c rtcw_clipbank.c rtcw_animscript.c rtcw_preview.c rtcw_rig_main.c rtcw_vfs_material.c rtcw_attach.c rtcw_contact.c rtcw_contact_color.c rtcw_visibility.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c rtcw_vfs.c rtcw_pk3.c -o rtcw_rig.exe
if errorlevel 1 exit /b 1
rem v0.14 JPEG/TGA standalone validation
 gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_texture.c rtcw_jpeg.c rtcw_imagecheck_main.c -o rtcw_imagecheck.exe
if errorlevel 1 exit /b 1
echo 14 ejecutables generados (v0.14).

rem v0.15 material diagnostic CLI
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_material.c rtcw_matprobe_main.c -o rtcw_matprobe.exe
if errorlevel 1 exit /b 1
echo 15 ejecutables generados (v0.15).

rem v0.16 first-stage shader diagnostics
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_shaderstyle.c rtcw_shaderprobe_main.c -o rtcw_shaderprobe.exe
if errorlevel 1 exit /b 1
echo 16 ejecutables generados (v0.16).

rem v0.17 bounded multi-stage shader diagnostics
gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_stagefx.c rtcw_stagefx_main.c -o rtcw_fxprobe.exe
if errorlevel 1 exit /b 1
echo 17 ejecutables generados (v0.17).

rem v0.19 deterministic model + clips + material QC audit
 gcc -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c rtcw_audit.c rtcw_audit_main.c rtcw_clipbank.c rtcw_vfs_material.c rtcw_material.c rtcw_texcache.c rtcw_stagefx.c rtcw_texture.c rtcw_jpeg.c rtcw_vfs.c rtcw_pk3.c -o rtcw_audit.exe
if errorlevel 1 exit /b 1
echo 18 herramientas generadas (v0.19).
