@echo off
rem Build and run the 32-bit self test of the txtblt Hangul fold helper
rem usage: build_txtblt_fold_test.bat <output dir>
setlocal
if "%~1"=="" (echo usage: build_txtblt_fold_test.bat ^<output dir^> & exit /b 2)
set OUT=%~1
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
set ROOT=%~dp0..
cl /nologo /c /TC /Oi- /GS- /Fo"%OUT%\txtblt_fold.obj" "%ROOT%\Extensions\TxtbltKorean\txtblt_fold.c" || exit /b 1
cl /nologo /EHsc /W3 /MT /D_CRT_SECURE_NO_WARNINGS /Fo"%OUT%\\" /Fe"%OUT%\TxtbltFoldTest.exe" "%~dp0TxtbltFoldTest.cpp" "%OUT%\txtblt_fold.obj" || exit /b 1
"%OUT%\TxtbltFoldTest.exe"
exit /b %ERRORLEVEL%
