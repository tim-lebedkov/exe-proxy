echo on

rem This script is used by AppVeyor to build the project.

set initial_path=%path%

where appveyor
where cmake

set version=%APPVEYOR_BUILD_VERSION:~0,-3%

SET NPACKD_CL=C:\Program Files\NpackdCL

set where=c:\Builds\exe-proxy-32-minsizerel
mkdir %where%
cd %where%
set path=C:\msys64\mingw32\bin;C:\Program Files (x86)\CMake\bin
cmake C:\projects\exe-proxy -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel -DWITH_JAVASCRIPT=OFF
if %errorlevel% neq 0 exit /b %errorlevel%
mingw32-make.exe -j 2
if %errorlevel% neq 0 exit /b %errorlevel%
C:\msys64\mingw32\bin\strip exeproxy.exe
if %errorlevel% neq 0 exit /b %errorlevel%

set where=c:\Builds\exe-proxy-32-minsizerel-js
mkdir %where%
cd %where%
set path=C:\msys64\mingw32\bin;C:\Program Files (x86)\CMake\bin
cmake C:\projects\exe-proxy -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel -DWITH_JAVASCRIPT=ON
if %errorlevel% neq 0 exit /b %errorlevel%
mingw32-make.exe -j 2
if %errorlevel% neq 0 exit /b %errorlevel%
C:\msys64\mingw32\bin\strip exeproxy.exe
if %errorlevel% neq 0 exit /b %errorlevel%

set where=c:\Builds\exe-proxy-64-minsizerel
mkdir %where%
cd %where%
set path=C:\msys64\mingw64\bin;C:\Program Files (x86)\CMake\bin
cmake C:\projects\exe-proxy -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel -DWITH_JAVASCRIPT=OFF
if %errorlevel% neq 0 exit /b %errorlevel%
mingw32-make.exe -j 2
if %errorlevel% neq 0 exit /b %errorlevel%
C:\msys64\mingw64\bin\strip exeproxy.exe
if %errorlevel% neq 0 exit /b %errorlevel%

set where=c:\Builds\exe-proxy-64-minsizerel-js
mkdir %where%
cd %where%
set path=C:\msys64\mingw64\bin;C:\Program Files (x86)\CMake\bin
cmake C:\projects\exe-proxy -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel -DWITH_JAVASCRIPT=ON
if %errorlevel% neq 0 exit /b %errorlevel%
mingw32-make.exe -j 2
if %errorlevel% neq 0 exit /b %errorlevel%
C:\msys64\mingw64\bin\strip exeproxy.exe
if %errorlevel% neq 0 exit /b %errorlevel%

set where=c:\release
mkdir %where%
cd %where%
copy c:\Builds\exe-proxy-32-minsizerel\exeproxy.exe exeproxy.exe
if %errorlevel% neq 0 exit /b %errorlevel%
copy c:\Builds\exe-proxy-32-minsizerel-js\exeproxy.exe exeproxy-js.exe
if %errorlevel% neq 0 exit /b %errorlevel%
copy c:\Builds\exe-proxy-64-minsizerel\exeproxy.exe exeproxy64.exe
if %errorlevel% neq 0 exit /b %errorlevel%
copy c:\Builds\exe-proxy-64-minsizerel-js\exeproxy.exe exeproxy-js64.exe
if %errorlevel% neq 0 exit /b %errorlevel%
copy "C:\projects\exe-proxy\README.md" .
if %errorlevel% neq 0 exit /b %errorlevel%
copy "C:\projects\exe-proxy\LICENSE.txt" .
if %errorlevel% neq 0 exit /b %errorlevel%
mkdir C:\Artifacts
if %errorlevel% neq 0 exit /b %errorlevel%
set path=%initial_path%
7z a C:\Artifacts\exeproxy-%version%.zip * -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact C:\Artifacts\exeproxy-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%


