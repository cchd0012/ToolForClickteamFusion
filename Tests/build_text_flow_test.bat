@echo off
rem Build and run the text flow analysis test (FrameEventTextFlow) with MSVC.
rem usage: build_text_flow_test.bat ^<output dir^>
setlocal
if "%~1"=="" (echo usage: build_text_flow_test.bat ^<output dir^> & exit /b 2)
set OUT=%~1
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set ROOT=%~dp0..
set SRC=%ROOT%\ToolForClickteamFusion
cl /nologo /EHsc /W3 /O2 /MT /D_CRT_SECURE_NO_WARNINGS ^
  /I"%ROOT%\includes\src" /I"%SRC%" /I"%ROOT%\includes\src\directx-sdk-june2010" ^
  /Fo"%OUT%\\" /Fe"%OUT%\TextFlowTest.exe" ^
  "%ROOT%\Tests\TextFlowTest.cpp" ^
  "%SRC%\Chunk\Frame\FrameEvent\FrameEventTextFlow.cpp" ^
  "%SRC%\Util\CommonUtil.cpp" ^
  "%ROOT%\includes\src\jsoncpp\jsoncpp.cpp"
if errorlevel 1 exit /b 1
"%OUT%\TextFlowTest.exe"
exit /b %ERRORLEVEL%
