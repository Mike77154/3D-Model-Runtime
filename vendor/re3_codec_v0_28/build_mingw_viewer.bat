@echo off
setlocal
if "%CC%"=="" set CC=gcc
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude tools\re3emd_viewer.c src\re3_codec.c -o re3emd_viewer.exe
if errorlevel 1 exit /b 1
echo re3emd_viewer.exe OK
endlocal
