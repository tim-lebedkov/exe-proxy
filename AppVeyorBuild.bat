echo on

rem This script is used by AppVeyor to build the project.

set initial_path=%path%

where appveyor
where cmake

set version=%APPVEYOR_BUILD_VERSION:~0,-2%

SET NPACKD_CL=C:\Program Files\NpackdCL

set where=c:\Builds\exe-proxy-32-minsizerel
mkdir %where%
cd %where%
set path=C:\msys64\mingw32\bin;C:\Program Files (x86)\CMake\bin
cmake C:\projects\exe-proxy -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel
if %errorlevel% neq 0 exit /b %errorlevel%
mingw32-make.exe -j 2
if %errorlevel% neq 0 exit /b %errorlevel%

set where=c:\Builds\exe-proxy-64-minsizerel
mkdir %where%
cd %where%
set path=C:\msys64\mingw64\bin;C:\Program Files (x86)\CMake\bin
cmake C:\projects\exe-proxy -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel
if %errorlevel% neq 0 exit /b %errorlevel%
mingw32-make.exe -j 2
if %errorlevel% neq 0 exit /b %errorlevel%

set path=%initial_path%

appveyor PushArtifact exeproxy\build\exeproxy%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%


