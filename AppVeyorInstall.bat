
echo on

rem This script is used by AppVeyor automatic builds to install the necessary
rem software dependencies.

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

"%npackd_cl%\ncl" add -p com.oracle.JDK64
if %errorlevel% neq 0 exit /b %errorlevel%

rem update all packages to the newest versions
rem MSYS2 repositories are currently not available
rem C:\msys64\usr\bin\pacman -Syu --noconfirm 
rem C:\msys64\usr\bin\pacman -Syu --noconfirm 

