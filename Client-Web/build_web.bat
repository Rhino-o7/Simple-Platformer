@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO_ROOT=%%~fI"
set "EMSDK_DIR=%SCRIPT_DIR%emsdk"
set "CONFIG=%~1"

if "%CONFIG%"=="" set "CONFIG=Release"

set "BUILD_DIR=%REPO_ROOT%\build\bin\%CONFIG%\Client-Web"
set "RESP_FILE=%BUILD_DIR%\sources.rsp"
set "FLECS_OBJ=%BUILD_DIR%\flecs.o"

if exist "%EMSDK_DIR%\emsdk_env.bat" (
    call "%EMSDK_DIR%\emsdk_env.bat" >nul
)

if defined EMSDK (
    if exist "%EMSDK%\emsdk_env.bat" call "%EMSDK%\emsdk_env.bat" >nul
)

where em++ >nul 2>&1
if errorlevel 1 (
    echo [ERROR] em++ not found in PATH.
    echo         Run emsdk_env.bat first, or keep emsdk in Client-Web\emsdk.
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
del "%RESP_FILE%" 2>nul

powershell -NoProfile -ExecutionPolicy Bypass -Command "$client = Get-ChildItem -Path '%SCRIPT_DIR%src' -Recurse -Filter *.cpp | Where-Object { $_.FullName -notmatch '\\web_client_runtime.cpp$' } | ForEach-Object { ($_.FullName -replace '\\','/') }; $core = Get-ChildItem -Path '%REPO_ROOT%\CoreLib\src' -Recurse -Filter *.cpp | Where-Object { $_.FullName -notmatch '\\data\\model.cpp$' } | ForEach-Object { ($_.FullName -replace '\\','/') }; ($client + $core) | Set-Content -Path '%RESP_FILE%'"

if not exist "%RESP_FILE%" (
    echo [ERROR] Failed to generate source list.
    exit /b 1
)

findstr /I /C:"web_client_runtime.cpp" "%RESP_FILE%" >nul
if not errorlevel 1 (
    echo [ERROR] sources.rsp unexpectedly contains web_client_runtime.cpp
    exit /b 1
)

set "COMMON_FLAGS=-std=c++20 -include thread -include mutex -include condition_variable -include functional -sUSE_GLFW=3 -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1 -sUSE_FREETYPE=1 -sASYNCIFY -sSTACK_SIZE=8388608 -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 -lwebsocket.js"
set "INCLUDE_FLAGS=-I%SCRIPT_DIR%include -I%SCRIPT_DIR%src -I%REPO_ROOT%\CoreLib\include -I%REPO_ROOT%\CoreLib\src -I%REPO_ROOT%\CoreLib\src\game -I%REPO_ROOT%\CoreLib\external -I%REPO_ROOT%\CoreLib\external\rapidjson-1.1.0\include -I%REPO_ROOT%\CoreLib\external\flecs-4.1.5\include -I%REPO_ROOT%\CoreLib\external\freetype\freetype-2.14.3\include -I%REPO_ROOT%\CoreLib\external\glm -I%REPO_ROOT%\CoreLib\external\GLFW\include -I%REPO_ROOT%\CoreLib\external\GLEW\include -I%REPO_ROOT%\CoreLib\external\websocketpp-0.8.2 -I%REPO_ROOT%\CoreLib\external\asio-1.12.1\include"
set "PRELOAD_FLAGS=--use-preload-cache --use-preload-plugins --preload-file %REPO_ROOT%\Client\vpg.cfg@/Client/vpg.cfg --preload-file %REPO_ROOT%\Client\data@/Client/data --preload-file %SCRIPT_DIR%data@/Client/data"
set "OUT_FILE=%BUILD_DIR%\Client-Web.html"

if /I "%CONFIG%"=="Debug" (
    set "OPT_FLAGS=-O0 -gsource-map -sASSERTIONS=2"
) else (
    set "OPT_FLAGS=-O2 -sASSERTIONS=1"
)

echo [INFO] Building Client-Web (%CONFIG%)...
call emcc -std=c11 -O2 -c "%REPO_ROOT%\CoreLib\external\flecs-4.1.5\distr\flecs.c" -o "%FLECS_OBJ%"
if errorlevel 1 (
    echo [ERROR] flecs.c build failed.
    exit /b 1
)

call em++ @"%RESP_FILE%" "%FLECS_OBJ%" %COMMON_FLAGS% %OPT_FLAGS% %INCLUDE_FLAGS% %PRELOAD_FLAGS% -o "%OUT_FILE%"
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [OK] Build complete: "%OUT_FILE%"
echo [TIP] Run in browser with: emrun "%OUT_FILE%"
exit /b 0
