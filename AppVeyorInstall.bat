
echo on

rem This script is used by AppVeyor automatic builds to install the necessary
rem software dependencies.

echo %NUMBER_OF_PROCESSORS%

msiexec.exe /qn /i https://github.com/tim-lebedkov/npackd-cpp/releases/download/version_1.25/NpackdCL64-1.25.0.msi
if %errorlevel% neq 0 exit /b %errorlevel%

SET NPACKD_CL=C:\Program Files\NpackdCL
"%npackd_cl%\ncl" set-repo -u  https://www.npackd.org/rep/recent-xml -u https://www.npackd.org/rep/xml?tag=stable -u https://www.npackd.org/rep/xml?tag=stable64 -u https://www.npackd.org/rep/xml?tag=libs
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" detect
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" set-install-dir -f "C:\Program Files (x86)"
if %errorlevel% neq 0 exit /b %errorlevel%

rem Python will be detected, but needs NpackdCL
"%npackd_cl%\ncl" add -p com.googlecode.windows-package-manager.NpackdCL
if %errorlevel% neq 0 exit /b %errorlevel%

rem update all packages to the newest versions
C:\msys64\usr\bin\pacman -Syu --noconfirm 
C:\msys64\usr\bin\pacman -Syu --noconfirm 

if %bits% equ 64 goto bits64

C:\msys64\usr\bin\pacman -S --noconfirm mingw-w64-i686-libtool
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

:bits64
C:\msys64\usr\bin\pacman -S --noconfirm mingw-w64-x86_64-libtool
if %errorlevel% neq 0 exit /b %errorlevel%

if "%prg%" neq "npackd" goto end

:end

