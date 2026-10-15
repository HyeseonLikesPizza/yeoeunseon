@echo off
setlocal
for %%I in ("%~dp0..\..\..\..\..") do set "studyRoot=%%~fI"
if not exist "%studyRoot%\.obsidian\" exit /b 3
set "studyOutput=%studyRoot%\tmp\"
if not exist "%studyOutput%" mkdir "%studyOutput%"
if exist "%studyOutput%vtune_hardware_verified_01" exit /b 2
powershell.exe -NoProfile -Command "$id = [Security.Principal.WindowsIdentity]::GetCurrent(); $p = New-Object Security.Principal.WindowsPrincipal($id); Write-Output ('Administrator=' + $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))" > "%studyOutput%VTuneHardwareCheck.log" 2>&1
"C:\Program Files (x86)\Intel\oneAPI\vtune\2026.4\bin64\vtune.exe" -collect uarch-exploration -result-dir "%studyOutput%vtune_hardware_verified_01" -- "%studyOutput%VectorVsHashFactors.exe" profile vector 8 50000 3 >> "%studyOutput%VTuneHardwareCheck.log" 2>&1
echo ExitCode=%ERRORLEVEL% >> "%studyOutput%VTuneHardwareCheck.log"
