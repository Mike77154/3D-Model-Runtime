@echo off
setlocal
if "%~3"=="" (
 echo Uso: demo_weighted_re3.bat archivo.EMD archivo.TIM carpeta_salida [clip]
 echo Ejemplo: demo_weighted_re3.bat EM30.EMD EM30.TIM comparacion 4
 exit /b 2
)
set "EMD=%~1"
set "TIM=%~2"
set "OUT=%~3"
set "CLIP=%~4"
if "%CLIP%"=="" set "CLIP=4"
if not exist "%OUT%" mkdir "%OUT%"
for %%W in (0 1024 2048 3072 4096) do (
 if not exist "%OUT%\peso_%%W" mkdir "%OUT%\peso_%%W"
 re3emd_anim_preview.exe "%EMD%" "%TIM%" "%OUT%\peso_%%W" 0 %CLIP% 1 quarter texture model main rest no-map - native pc:0:%%W
 if errorlevel 1 exit /b 1
)
if not exist "%OUT%\pulso" mkdir "%OUT%\pulso"
re3emd_anim_preview.exe "%EMD%" "%TIM%" "%OUT%\pulso" 0 %CLIP% 20 quarter texture model main rest no-map - native pc-pulse:0
if errorlevel 1 exit /b 1
re3emd_weighted_morph_audit.exe "%EMD%"
if errorlevel 1 exit /b 1
echo OK: fotogramas BMP generados en "%OUT%"
endlocal
