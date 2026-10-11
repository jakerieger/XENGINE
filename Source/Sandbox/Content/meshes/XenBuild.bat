@echo off
rem Builds a Xen project from XED (or by hand). Run from the project root:
rem
rem   XenBuild.bat module    the game module XED loads        (preset 'editor')
rem   XenBuild.bat debug     the game, Debug build            (preset 'debug')
rem   XenBuild.bat release   the game, Release distribution   (preset 'release')
rem
rem The preset is configured first if its build tree doesn't exist yet. Exits
rem non-zero on any failure, so a caller can tell a build succeeded.

set "TARGET=%~1"
if /i "%TARGET%"=="module"  (set "PRESET=editor"  & goto :have_preset)
if /i "%TARGET%"=="debug"   (set "PRESET=debug"   & goto :have_preset)
if /i "%TARGET%"=="release" (set "PRESET=release" & goto :have_preset)
echo Usage: XenBuild.bat ^<module ^| debug ^| release^>
exit /b 2

:have_preset
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (echo Visual Studio was not found - install it or run XED from a developer prompt & exit /b 1)
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

if not exist "build\%PRESET%\CMakeCache.txt" (cmake --preset %PRESET% || exit /b 1)
cmake --build --preset %PRESET% || exit /b 1
