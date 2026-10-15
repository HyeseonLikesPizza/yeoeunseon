@echo off
setlocal
for %%I in ("%~dp0..\..\..\..\..") do set "studyRoot=%%~fI"
if not exist "%studyRoot%\.obsidian\" exit /b 3
set "studyOutput=%studyRoot%\tmp\"
if not exist "%studyOutput%" mkdir "%studyOutput%"
call :capture vector 8 1
if errorlevel 1 exit /b 1
call :capture map 8 1
if errorlevel 1 exit /b 1
call :capture vector 256 1
if errorlevel 1 exit /b 1
call :capture map 256 1
if errorlevel 1 exit /b 1
call :capture map 256 2
if errorlevel 1 exit /b 1
call :capture vector 256 2
if errorlevel 1 exit /b 1
call :capture map 8 2
if errorlevel 1 exit /b 1
call :capture vector 8 2
exit /b %ERRORLEVEL%
:capture
set "result=%studyOutput%vtune_memory_%1_%2_r%3"
if exist "%result%" exit /b 2
echo START %1 %2 round=%3 >> "%studyOutput%VTuneMemoryCaptures.log"
"C:\Program Files (x86)\Intel\oneAPI\vtune\2026.4\bin64\vtune.exe" -collect uarch-exploration -finalization-mode=none -result-dir "%result%" -- "%studyOutput%VectorMemoryProfile.exe" profile %1 %2 50000 5 0 >> "%studyOutput%VTuneMemoryCaptures.log" 2>&1
if errorlevel 1 exit /b 1
echo DONE %1 %2 round=%3 >> "%studyOutput%VTuneMemoryCaptures.log"
exit /b 0
