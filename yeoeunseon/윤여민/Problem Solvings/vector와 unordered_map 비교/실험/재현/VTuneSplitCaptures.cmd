@echo off
setlocal
for %%I in ("%~dp0..\..\..\..\..") do set "studyRoot=%%~fI"
if not exist "%studyRoot%\.obsidian\" exit /b 3
set "studyOutput=%studyRoot%\tmp\"
if not exist "%studyOutput%" mkdir "%studyOutput%"
call :capture aos256 1
if errorlevel 1 exit /b 1
call :capture ids4 1
if errorlevel 1 exit /b 1
call :capture ids4 2
if errorlevel 1 exit /b 1
call :capture aos256 2
exit /b %ERRORLEVEL%
:capture
set "result=%studyOutput%vtune_split_%1_r%2"
if exist "%result%" exit /b 2
echo START %1 round=%2 >> "%studyOutput%VTuneSplitCaptures.log"
"C:\Program Files (x86)\Intel\oneAPI\vtune\2026.4\bin64\vtune.exe" -collect uarch-exploration -finalization-mode=none -result-dir "%result%" -- "%studyOutput%VectorSplitProfile.exe" profile-split %1 5 0 >> "%studyOutput%VTuneSplitCaptures.log" 2>&1
if errorlevel 1 exit /b 1
echo DONE %1 round=%2 >> "%studyOutput%VTuneSplitCaptures.log"
exit /b 0
