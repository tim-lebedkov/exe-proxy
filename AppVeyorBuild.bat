echo on

rem This script is used by AppVeyor to build the project.

set initial_path=%path%

where appveyor
where cmake

set version=%APPVEYOR_BUILD_VERSION:~0,-4%

SET NPACKD_CL=C:\Program Files\NpackdCL

set mingw_libs=i686-w64-mingw32
set mingw=C:\msys64\mingw32

set mingw=C:\msys64\mingw64

mkdir exe-proxy\build
if %errorlevel% neq 0 exit /b %errorlevel%

pushd exe-proxy\build
set path=%mingw%\bin;C:\Program Files (x86)\CMake\bin
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%

cmake ..\ -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=..\install
if %errorlevel% neq 0 exit /b %errorlevel%

cmake -LAH

mingw32-make.exe -j 2 install
if %errorlevel% neq 0 exit /b %errorlevel%


del ..\install\tests.exe
if %errorlevel% neq 0 exit /b %errorlevel%

del ..\install\ftests.exe
if %errorlevel% neq 0 exit /b %errorlevel%

C:\Windows\System32\xcopy.exe ..\install ..\install-debug /E /I /H /Y
if %errorlevel% neq 0 exit /b %errorlevel%

strip ..\install\exeproxy.exe
if %errorlevel% neq 0 exit /b %errorlevel%

pushd ..\install

7z a ..\build\exeproxy%bits%-%version%.zip * -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%
	   
popd
popd

set path=%initial_path%

appveyor PushArtifact exeproxy\build\exeproxy%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact exeproxy\install\exeproxy%bits%-%version%.msi
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact exeproxy\build\exeproxy%bits%-debug-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%


