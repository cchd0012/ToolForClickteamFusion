@echo off
rem Build the 32-bit helper test for txtblt_font.c
rem usage: build_txtblt_font_test.bat <output dir>
setlocal
set OUT=%~1
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /c /Oi- /GS- /Zl /Gy- /Fo"%OUT%\txtblt_font_t.obj" "%~dp0..\Extensions\TxtbltFont\txtblt_font.c" || exit /b 1
cl /nologo /EHsc /Fo"%OUT%\\" /Fe"%OUT%\TxtbltFontTest.exe" "%~dp0TxtbltFontTest.cpp" "%OUT%\txtblt_font_t.obj" || exit /b 1
exit /b 0
