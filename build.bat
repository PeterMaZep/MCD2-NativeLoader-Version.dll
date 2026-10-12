@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul && goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (
  echo Visual Studio with the C++ build tools was not found
  exit /b 1
)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
:build
if not exist obj mkdir obj
ml64 /nologo /c /Fo obj\version_exports.obj version_exports.asm || exit /b 1
cl /nologo /O2 /LD /MT /W3 /DUNICODE /D_UNICODE /Foobj\ version_proxy.c loader.c console.c engine.c bridge.c obj\version_exports.obj user32.lib gdi32.lib dwmapi.lib uxtheme.lib comdlg32.lib ole32.lib /Fe:version.dll /link /IMPLIB:obj\version.lib /MANIFEST:EMBED /MANIFESTDEPENDENCY:"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'" || exit /b 1
