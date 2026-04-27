@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

cd /d "%~dp0"

if defined MINGW_PREFIX goto :have_prefix

for %%P in ("D:\msys2\ucrt64" "C:\msys64\ucrt64" "D:\msys2\mingw64" "C:\msys64\mingw64") do (
  if exist "%%~P\bin\g++.exe" (
    set "MINGW_PREFIX=%%~P"
    goto :have_prefix
  )
)

where g++ >nul 2>&1
if errorlevel 1 (
  echo 未找到 g++。请安装 MSYS2 UCRT64 或 MinGW64，或设置 MINGW_PREFIX。
  exit /b 1
)

for /f "delims=" %%I in ('where g++ 2^>nul') do (
  set "GPP_BIN=%%~dpI"
  goto :have_gpp
)
echo 无法定位 g++。
exit /b 1

:have_gpp
pushd "%GPP_BIN%.."
set "MINGW_PREFIX=!CD!"
popd

:have_prefix
set "PATH=%MINGW_PREFIX%\bin;%SystemRoot%\system32;%SystemRoot%"
REM 用户目录含中文时 %%TEMP%% 可能导致 gcc/as 无法创建 .o（No such file or directory）
set "BUILD_TMP=%~dp0build_tmp"
if not exist "%BUILD_TMP%" mkdir "%BUILD_TMP%"
set "TEMP=%BUILD_TMP%"
set "TMP=%BUILD_TMP%"

set "GPP_EXE=%MINGW_PREFIX%\bin\g++.exe"
if not exist "%GPP_EXE%" (
  echo 未找到编译器: %GPP_EXE%
  exit /b 1
)

set "CXXFLAGS=-std=c++17 -Wall -O2 -I%MINGW_PREFIX%/include/SDL2"
set "LDFLAGS=-L%MINGW_PREFIX%\lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf"

REM 错误若仍指向 mingw64\...\cc1plus：实际加载的是 mingw64 里的 cc1plus，必须把 **mingw64 自己 bin 里的 zlib1.dll**
REM 同步到 **mingw64** 的 lib\gcc\...\ 下。仅同步 ucrt64 不够。
if not defined SKIP_ZLIB_FIX (
  echo [修复] 将各前缀 bin\zlib1.dll 同步到: lib\gcc\...\版本\ 以及 x86_64-w64-mingw32\bin\
  echo       ^(gcc 调用的 as.exe 在此目录，缺 zlib 会报 uncompress2 / crc32_combine 等^)
  if exist "%MINGW_PREFIX%\bin\zlib1.dll" (
    echo          %MINGW_PREFIX% ^(当前编译^)
    call :sync_zlib_root "%MINGW_PREFIX%"
  )
  for %%R in ("D:\msys2\ucrt64" "C:\msys64\ucrt64" "D:\msys2\mingw64" "C:\msys64\mingw64") do (
    if /i not "%%~R"=="%MINGW_PREFIX%" (
      if exist "%%~R\bin\zlib1.dll" (
        echo          %%~R
        call :sync_zlib_root "%%~R"
      )
    )
  )
  echo.
) else (
  echo [跳过] SKIP_ZLIB_FIX=1
  echo.
)

set "CC1_OUT=%BUILD_TMP%\cc1diag.txt"
REM GCC 15 需 -print-prog-name=cc1plus 整体；用文件避免 for /f 把 = 拆开
"%GPP_EXE%" -print-prog-name^=cc1plus >"%CC1_OUT%" 2>&1
set "CC1PLUS="
for /f "usebackq delims=" %%P in ("%CC1_OUT%") do (
  set "CC1PLUS=%%P"
  goto :cc1_done
)
:cc1_done
echo [诊断] 本 g++ 实际调用的 cc1plus:
if "!CC1PLUS!"=="" (
  echo          ^(无输出 — 见 %CC1_OUT%^)
  echo          手动: "%GPP_EXE%" -print-prog-name^=cc1plus
) else (
  echo          !CC1PLUS!
)
echo %GPP_EXE% | findstr /i "\\ucrt64\\" >nul
if not errorlevel 1 if not "!CC1PLUS!"=="" (
  echo !CC1PLUS! | findstr /i "\\mingw64\\" >nul
  if not errorlevel 1 (
    echo.
    echo *** 异常混用: g++ 来自 ucrt64，但 cc1plus 在 mingw64 树下 ***
    echo *** 请打开「MSYS2 UCRT64」终端执行:
    echo     pacman -S --overwrite=* mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-zlib
    echo *** 或暂时只用 MinGW64 编译: set MINGW_PREFIX=D:\msys2\mingw64
    echo.
  )
)
echo MINGW_PREFIX: %MINGW_PREFIX%
echo 编译器:     %GPP_EXE%
echo.

echo 正在编译...
pushd "%MINGW_PREFIX%\bin"
"%GPP_EXE%" %CXXFLAGS% "%~dp0src\main.cpp" -o "%~dp0path_planning_sim.exe" %LDFLAGS%
set "ERR=%ERRORLEVEL%"
popd
if not "%ERR%"=="0" (
  echo.
  echo ===== 编译失败 =====
  echo SDL2: UCRT64 用 mingw-w64-ucrt-x86_64-SDL2 / SDL2_ttf ；MinGW64 用 mingw-w64-x86_64-*
  echo 若曾弹窗 zlib 入口点错误: 试 compile_clang.bat 或在 MSYS2 终端里直接执行 g++ 命令查看完整输出
  pause
  exit /b 1
)

echo 编译成功。
echo 正在复制运行依赖 DLL 到 exe 同目录 ^(避免 Windows 加载到其它目录里旧版 zlib/SDL，导致 0xC0000139 入口点错误^) ...
call :copy_runtime_dlls "%MINGW_PREFIX%" "%~dp0"
echo 运行 path_planning_sim.exe ...
"%~dp0path_planning_sim.exe"
set "RUNERR=%ERRORLEVEL%"
if not "%RUNERR%"=="0" (
  echo.
  echo 程序已退出，代码 %RUNERR%
  echo 若为 -1073741511 ^(0xC0000139^)：某 DLL 入口点不匹配。请确认本目录已有 SDL2.dll/SDL2_ttf.dll 等；或暂时关闭杀毒后重试。
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

:sync_zlib_root
REM collect2 调用 prefix\x86_64-w64-mingw32\bin\as.exe ，旁若缺/旧 zlib1.dll 会报 uncompress2 等
if exist "%~1\x86_64-w64-mingw32\bin\." (
  copy /Y "%~1\bin\zlib1.dll" "%~1\x86_64-w64-mingw32\bin\" >nul 2>&1
)
if exist "%~1\i686-w64-mingw32\bin\." (
  copy /Y "%~1\bin\zlib1.dll" "%~1\i686-w64-mingw32\bin\" >nul 2>&1
)
for /d %%T in ("%~1\lib\gcc\*") do (
  for /d %%V in ("%%T\*") do (
    copy /Y "%~1\bin\zlib1.dll" "%%V\" >nul 2>&1
  )
)
goto :eof
