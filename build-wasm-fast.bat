@echo off
REM BracketSim "Run on my PC" engine (29 Sep 2026): the same engine as build-cli, compiled to WebAssembly for speed
REM (-O2) rather than size (build-wasm-min is -Oz). Single-threaded: the page runs one engine per Web Worker.
REM   %OUT%.bat           configure (first time) + incremental build
setlocal
REM emsdk_env.bat prints bash exports when started from Git Bash and sets nothing, so the environment is set here.
set EMSDK=%~dp0..\emsdk
set EM_CONFIG=%EMSDK%\.emscripten
set EMSDK_PYTHON=%EMSDK%\python\3.13.3_64bit\python.exe
set EMSDK_NODE=%EMSDK%\node\24.19.0_64bit\node.exe
set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%EMSDK%;%EMSDK%\upstream\emscripten;%EMSDK%\node\24.19.0_64bit;%EMSDK%\python\3.13.3_64bit;%PATH%
set NINJA=C:\PROGRA~1\MIB055~1\2022\COMMUN~1\Common7\IDE\COMMON~1\MICROS~1\CMake\Ninja\ninja.exe
cd /d "%~dp0"
REM 29 Sep: native wasm exceptions (-fwasm-exceptions) - -fexceptions emulates them in JS and ran 3.5-4x slower than
REM native; supported by Chrome 95+, Firefox 100+, Safari 15.2+.
REM 1 Oct 2026: -DNDEBUG. CMAKE_CXX_FLAGS_RELEASE=-O3 replaced CMake's "-O3 -DNDEBUG", so every assert() in the
REM engine ran in the browser build (native has /DNDEBUG): 1.8x slower, same numbers. New build dir for the new flags.
set OUT=build-wasm-nd
if not exist %OUT%\build.ninja (
  cmake -G Ninja -S . -B %OUT% ^
    -DCMAKE_MAKE_PROGRAM=%NINJA% ^
    -DCMAKE_TOOLCHAIN_FILE=%~dp0..\emsdk\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DSC_NO_NETWORKING=ON -DSC_NO_THREADING=ON -DBUILD_GUI=OFF ^
    "-DCMAKE_CXX_FLAGS=-O3 -DNDEBUG -fwasm-exceptions -msimd128" ^
    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG" ^
    "-DCMAKE_EXE_LINKER_FLAGS=-O3 -sMODULARIZE=1 -sEXPORT_NAME=createBracketSimC -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,worker,node -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=callMain,FS -sASSERTIONS=0 -fwasm-exceptions -sMALLOC=mimalloc"
  if errorlevel 1 exit /b 1
)
"%NINJA%" -C %OUT% -j 6 simc
