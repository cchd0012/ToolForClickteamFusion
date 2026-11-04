@echo off
rem Build and run the diagnosis logic test (Diagnosis/DiagnosisBuild.cpp) with MSVC.
rem usage: build_diagnosis_test.bat ^<output dir^>
setlocal
if "%~1"=="" (echo usage: build_diagnosis_test.bat ^<output dir^> & exit /b 2)
set OUT=%~1
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set ROOT=%~dp0..
set SRC=%ROOT%\ToolForClickteamFusion
cl /nologo /utf-8 /EHsc /W3 /O2 /MT /D_CRT_SECURE_NO_WARNINGS ^
  /I"%ROOT%\includes\src" /I"%SRC%" /I"%ROOT%\includes\src\directx-sdk-june2010" ^
  /Fo"%OUT%\\" /Fe"%OUT%\DiagnosisTest.exe" ^
  "%ROOT%\Tests\DiagnosisTest.cpp" ^
  "%SRC%\Diagnosis\DiagnosisBuild.cpp" ^
  "%ROOT%\includes\src\jsoncpp\jsoncpp.cpp"
if errorlevel 1 exit /b 1
"%OUT%\DiagnosisTest.exe"
exit /b %ERRORLEVEL%
