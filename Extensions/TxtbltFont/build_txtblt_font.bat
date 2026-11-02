@echo off
rem Compile txtblt_font.c to a 32-bit object (CRT-free, position independent code)
rem usage: build_txtblt_font.bat <output dir>
setlocal
set OUT=%~1
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /c /Oi- /GS- /Zl /Gy- /Fo"%OUT%\txtblt_font.obj" "%~dp0txtblt_font.c"
exit /b %ERRORLEVEL%
