@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

cd /d "%~dp0"

if defined MINGW_PREFIX goto :have_prefix

for %%P in ("D:\msys2\ucrt64" "C:\msys64\ucrt64" "D:\msys2\mingw64" "C:\msys64\mingw64") do (
  if exist "%%~P\bin\clang++.exe" (
    set "MINGW_PREFIX=%%~P"
    goto :have_prefix
  )
)

where clang++ >nul 2>&1
if errorlevel 1 (
  echo 未找到 clang++。
  echo UCRT64:  pacman -S mingw-w64-ucrt-x86_64-clang
  echo MinGW64: pacman -S mingw-w64-x86_64-clang
  exit /b 1
)

for /f "delims=" %%I in ('where clang++ 2^>nul') do (
  set "CL_BIN=%%~dpI"
  goto :have_cl
)
exit /b 1

:have_cl
pushd "%CL_BIN%.."
set "MINGW_PREFIX=!CD!"
popd

:have_prefix
set "PATH=%MINGW_PREFIX%\bin;%SystemRoot%\system32;%SystemRoot%"
set "BUILD_TMP=%~dp0build_tmp"
if not exist "%BUILD_TMP%" mkdir "%BUILD_TMP%"
set "TEMP=%BUILD_TMP%"
set "TMP=%BUILD_TMP%"

set "CLANG_EXE=%MINGW_PREFIX%\bin\clang++.exe"
if not exist "%CLANG_EXE%" (
  echo 未找到: %CLANG_EXE%
  exit /b 1
)

set "CXXFLAGS=-std=c++17 -Wall -O2 -I%MINGW_PREFIX%/include/SDL2"
set "LDFLAGS=-L%MINGW_PREFIX%\lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf"

echo MINGW_PREFIX: %MINGW_PREFIX%
echo Clang:        %CLANG_EXE%
echo 编译中...
pushd "%MINGW_PREFIX%\bin"
"%CLANG_EXE%" %CXXFLAGS% "%~dp0src\main.cpp" -o "%~dp0path_planning_sim.exe" %LDFLAGS%
set "ERR=%ERRORLEVEL%"
popd
if not "%ERR%"=="0" (
  echo 编译失败。请安装 SDL2/SDL2_ttf ^(UCRT64 用 mingw-w64-ucrt-x86_64-* 包^)
  exit /b 1
)

echo 正在复制运行依赖 DLL 到 exe 同目录 ...
call :copy_runtime_dlls "%MINGW_PREFIX%" "%~dp0"
echo 运行 path_planning_sim.exe ...
"%~dp0path_planning_sim.exe"
set "RUNERR=%ERRORLEVEL%"
if not "%RUNERR%"=="0" (
  echo 退出代码 %RUNERR% ^(-1073741511 多为 DLL 入口点/版本问题^)
  pause
)
goto :eof

:copy_runtime_dlls
set "MP=%~1"
set "OD=%~2"
for %%D in (
  SDL2.dll SDL2_ttf.dll zlib1.dll
  libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
  libfreetype-6.dll libharfbuzz-0.dll libpng16-16.dll libbz2-1.dll
  libbrotlidec.dll libbrotlicommon.dll libgraphite2.dll
  libintl-8.dll libiconv-2.dll libglib-2.0-0.dll libpcre2-8-0.dll libb2-1.dll
) do (
  if exist "%MP%\bin\%%D" copy /Y "%MP%\bin\%%D" "%OD%" >nul 2>&1
)
goto :eof
