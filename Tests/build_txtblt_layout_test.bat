@echo off
rem Build the 32-bit layout test for txtblt_font.c (kfont_layout)
rem usage: build_txtblt_layout_test.bat <output dir>
setlocal
set OUT=%~1
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /c /Oi- /GS- /Zl /Gy- /Fo"%OUT%\txtblt_font_l.obj" "%~dp0..\Extensions\TxtbltFont\txtblt_font.c" || exit /b 1
cl /nologo /EHsc /Fo"%OUT%\\" /Fe"%OUT%\TxtbltLayoutTest.exe" "%~dp0TxtbltLayoutTest.cpp" "%OUT%\txtblt_font_l.obj" || exit /b 1
exit /b 0
