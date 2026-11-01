@echo off
rem MSVC 명령줄 도구로 헤드리스 청크 모델 자체 테스트를 빌드한다 (최적화, release zlib 사용).
rem 사용법: build_chunk_model_test.bat <출력 폴더>
setlocal
if "%~1"=="" (echo usage: build_chunk_model_test.bat ^<output dir^> & exit /b 2)
set OUT=%~1
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set ROOT=%~dp0..
set SRC=%ROOT%\ToolForClickteamFusion
cl /nologo /EHsc /W3 /O2 /MT /D_CRT_SECURE_NO_WARNINGS ^
  /I"%ROOT%\includes\src" /I"%SRC%" /I"%ROOT%\includes\src\directx-sdk-june2010" ^
  /Fo"%OUT%\\" /Fe"%OUT%\ChunkModelTest.exe" ^
  "%ROOT%\Tests\ChunkModelTest.cpp" ^
  "%SRC%\Chunk\Frame\FrameEvent\FrameEventModel.cpp" ^
  "%SRC%\Chunk\Frame\FrameEvent\FrameEventText.cpp" ^
  "%SRC%\Util\CommonUtil.cpp" ^
  "%ROOT%\includes\src\jsoncpp\jsoncpp.cpp" ^
  "%SRC%\Chunk\Object\ObjectCommon.cpp" ^
  "%SRC%\Chunk\Object\ObjectText.cpp" ^
  "%SRC%\Chunk\TwoFivePlusObject\ObjectElementContainer.cpp" ^
  "%SRC%\Util\ZlibUtil.cpp" ^
  /link /LIBPATH:"%ROOT%\includes\lib\x64\Release" zlib.lib
exit /b %ERRORLEVEL%
