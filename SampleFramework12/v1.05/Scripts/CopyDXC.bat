@echo off

setlocal

set BUILD_TYPE=Release

if "%1"=="-Debug" (
  set BUILD_TYPE=Debug
)

xcopy %HLSL_SRC_DIR%\Include\dxc\dxcapi.h %~dp0..\..\..\Externals\DXCompiler\Include\ /y
xcopy %HLSL_BLD_DIR%\%BUILD_TYPE%\lib\dxcompiler.lib %~dp0..\..\..\Externals\DXCompiler\Lib\ /y
xcopy %HLSL_BLD_DIR%\%BUILD_TYPE%\bin\dxcompiler.* %~dp0..\..\..\Externals\DXCompiler\Bin\ /y
