@echo off
rem ParserUnicode 확장(parser.mfx 의 유니코드 버전)을 32비트로 빌드한다 (정적 CRT)
rem 사용법: build_parser_unicode.bat <출력 폴더>
setlocal
if "%~1"=="" (echo usage: build_parser_unicode.bat ^<output dir^> & exit /b 2)
set OUT=%~1
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /EHsc /W3 /O2 /MT /LD /D_CRT_SECURE_NO_WARNINGS /Fo"%OUT%\\" /Fe"%OUT%\parser.mfx" "%~dp0ParserUnicode.cpp" /link /DEF:"%~dp0ParserUnicode.def" /MACHINE:X86
exit /b %ERRORLEVEL%
