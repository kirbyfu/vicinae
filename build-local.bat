@echo off
rem Local Windows build. Usage: build-local.bat [configure|build|installer|env]
rem Toolchain: VS 2022 Build Tools, CMake/Ninja via mise (mise.local.toml), Qt via aqtinstall.
rem Paths are detected; to override any of them, create build-local.env.bat (gitignored) next to this file:
rem   set QTDIR=C:\Qt\6.10.1\msvc2022_64
rem   set MISE=C:\path\to\mise.exe
rem   set VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
setlocal
cd /d "%~dp0"
if exist "%~dp0build-local.env.bat" call "%~dp0build-local.env.bat"

rem Qt: defaults to .toolchains\Qt next to the repo checkout
if not defined QTDIR set "QTDIR=%~dp0..\.toolchains\Qt\6.10.1\msvc2022_64"
for %%I in ("%QTDIR%") do set "QTDIR=%%~fI"
if not exist "%QTDIR%\bin\qmake.exe" (
  echo Qt not found in %QTDIR%. Install it ^(see CLAUDE.local.md^) or set QTDIR in build-local.env.bat.
  exit /b 1
)

rem mise: PATH first, then the WinGet package folder (WinGet doesn't always put it on PATH)
if defined MISE goto :have_mise
for /f "delims=" %%I in ('where mise.exe 2^>nul') do if not defined MISE set "MISE=%%I"
if defined MISE goto :have_mise
for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\jdx.mise_*") do (
  if exist "%%D\mise\bin\mise.exe" set "MISE=%%D\mise\bin\mise.exe"
)
:have_mise
if not defined MISE (
  echo mise not found. Install it ^(winget install jdx.mise^) or set MISE in build-local.env.bat.
  exit /b 1
)

rem MSVC: the newest VS 2022 (17.x) install with the C++ x64 tools, matching CI
if defined VSDIR goto :have_vs
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%I in (`call "%%VSWHERE%%" -version [17.0^,18.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath`) do set "VSDIR=%%I"
)
:have_vs
if not exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" (
  echo Visual Studio 2022 with C++ tools not found. Install the Build Tools or set VSDIR in build-local.env.bat.
  exit /b 1
)

if "%1"=="env" goto :env

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
rem Qt-linked build tools (katehighlightingindexer) need Qt's DLLs
set "PATH=%QTDIR%\bin;%PATH%"

if "%1"=="configure" goto :configure
if "%1"=="installer" goto :installer
"%MISE%" exec -- cmake --build --preset windows-relwithdebinfo
exit /b %errorlevel%

:configure
rem /utf-8 passed globally: CMakeLists.txt adds it after the vendored libs, which breaks on non-UTF-8 codepages
"%MISE%" exec -- cmake --preset windows-relwithdebinfo "-DCMAKE_PREFIX_PATH=%QTDIR:\=/%" "-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /GR /EHsc /utf-8" "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS /utf-8"
exit /b %errorlevel%

:installer
"%MISE%" exec -- powershell -NoProfile -ExecutionPolicy Bypass -File scripts\mkinstaller.ps1 -BuildDir build-relwithdebinfo
exit /b %errorlevel%

:env
echo QTDIR=%QTDIR%
echo MISE=%MISE%
echo VSDIR=%VSDIR%
exit /b 0
