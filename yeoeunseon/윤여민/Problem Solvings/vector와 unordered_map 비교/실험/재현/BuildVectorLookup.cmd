@echo off
setlocal
if "%~1"=="" (
    set "studySource=%~dp0..\VectorVsHashLookup.cpp"
) else (
    set "studySource=%~f1"
)
for %%I in ("%~dp0..\..\..\..\..") do set "studyRoot=%%~fI"
if not exist "%studyRoot%\.obsidian\" exit /b 3
pushd "%studyRoot%"
if not exist "tmp" mkdir "tmp"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /O2 /EHsc /std:c++17 "%studySource%" /Fe:"tmp\VectorVsHashLookup.exe" /Fo:"tmp\VectorVsHashLookup.obj"
set "buildResult=%ERRORLEVEL%"
popd
exit /b %buildResult%
