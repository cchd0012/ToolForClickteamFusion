@echo off
rem Builds the headless chunk model self-test with the MSVC command line tools (optimised, release zlib).
rem usage: build_chunk_model_test.bat <output dir>
setlocal
if "%~1"=="" (echo usage: build_chunk_model_test.bat ^<output dir^> & exit /b 2)
set OUT=%~1
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set ROOT=%~dp0..
set SRC=%ROOT%\ToolForClickteamFusion
cl /nologo /EHsc /W3 /O2 /MT /D_CRT_SECURE_NO_WARNINGS ^
  /I"%ROOT%\includes\src" /I"%SRC%" /I"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include" ^
  /Fo"%OUT%\\" /Fe"%OUT%\ChunkModelTest.exe" ^
  "%ROOT%\Tests\ChunkModelTest.cpp" ^
  "%SRC%\Chunk\Frame\FrameEvent\FrameEventModel.cpp" ^
  "%SRC%\Chunk\Object\ObjectCommon.cpp" ^
  "%SRC%\Chunk\TwoFivePlusObject\ObjectElementContainer.cpp" ^
  "%SRC%\Util\ZlibUtil.cpp" ^
  /link /LIBPATH:"%ROOT%\includes\lib\x64\Release" zlib.lib
exit /b %ERRORLEVEL%
