@echo off
setlocal
for %%I in ("%~dp0..\..\..\..\..") do set "studyRoot=%%~fI"
if not exist "%studyRoot%\.obsidian\" exit /b 3
set "studyOutput=%studyRoot%\tmp\"
if not exist "%studyOutput%" mkdir "%studyOutput%"
rem Reverse the complete capture order in round 2. Never replace an old result.
call :capture vector periodic 1
if errorlevel 1 exit /b 1
call :capture map periodic 1
if errorlevel 1 exit /b 1
call :capture vector shuffled 1
if errorlevel 1 exit /b 1
call :capture map shuffled 1
if errorlevel 1 exit /b 1
call :capture map shuffled 2
if errorlevel 1 exit /b 1
call :capture vector shuffled 2
if errorlevel 1 exit /b 1
call :capture map periodic 2
if errorlevel 1 exit /b 1
call :capture vector periodic 2
exit /b %ERRORLEVEL%
:capture
set "result=%studyOutput%vtune_order_%1_%2_r%3"
if exist "%result%" exit /b 2
echo START %1 %2 round=%3 >> "%studyOutput%VTuneOrderCaptures.log"
"C:\Program Files (x86)\Intel\oneAPI\vtune\2026.4\bin64\vtune.exe" -collect uarch-exploration -finalization-mode=none -result-dir "%result%" -- "%studyOutput%VectorOrderProfile.exe" profile-order %1 %2 3 >> "%studyOutput%VTuneOrderCaptures.log" 2>&1
if errorlevel 1 exit /b 1
echo DONE %1 %2 round=%3 >> "%studyOutput%VTuneOrderCaptures.log"
exit /b 0
