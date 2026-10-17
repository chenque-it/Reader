@echo off

cd /d "%~dp0"
set "work_dir=%cd%"
set "sln_name=Reader"
set "publish=publish"
set "ret=0"
set "vers="

:: locate VsDevCmd via vswhere (no hardcoded VS path)
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" (
echo [ERROR] vswhere.exe not found. Please install Visual Studio first.
goto _err
)
set "vsdevcmd="
for /f "usebackq delims=" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.Component.MSBuild -find Common7\Tools\VsDevCmd.bat`) do set "vsdevcmd=%%i"
if not defined vsdevcmd (
echo [ERROR] VsDevCmd.bat not found via vswhere.
goto _err
)
echo [INFO] VsDevCmd : %vsdevcmd%

:: open vsdev
call "%vsdevcmd%"

setlocal enableextensions

:: build Release x64
call:func_build Release x64
if "%ret%" == "1" goto _err
call:func_get_version x64\Release
if "%vers%" == "" goto _err
call:func_copy_to_publish x64\Release %vers%
call:func_packet %vers%
call:func_cleanup x64\Release 1
call:func_cleanup x64 1


:: completed
:_completed
echo "SUCCESS: completed."
pause
exit

:: failed
:_err
echo "ERROR: failed."
pause
exit


:func_build
:: %1 is Release/Debug, %2 is x64
devenv %sln_name%.sln /rebuild "%1|%2"
if not exist "%work_dir%\%2\%1\%sln_name%.exe" (
set "ret=1"
)
goto:eof

:func_get_version
:: %1 is config
set "file=%work_dir%\%1\%sln_name%.exe"
if not exist "%file%" (
set "ret=1"
goto:eof
)
:: read file version via PowerShell (wmic was removed since Windows 11 24H2)
for /f "usebackq delims=" %%v in (`powershell -NoProfile -Command "(Get-Item -LiteralPath '%file%').VersionInfo.FileVersion"`) do set "vers=%%v"
if not defined vers set "ret=1"
goto:eof

:func_copy_to_publish
:: %1 is config, %2 is version, %3 need cleanup
if "%3" == "1" (
if exist "%work_dir%\%publish%\Reader_v%2" rd "%work_dir%\%publish%\Reader_v%2" /q /s
mkdir "%work_dir%\%publish%\Reader_v%2"
)
xcopy /y "%work_dir%\%1\%sln_name%.exe" "%work_dir%\%publish%\Reader_v%2\"
xcopy /y "%work_dir%\readme.txt" "%work_dir%\%publish%\Reader_v%2\"
goto:eof

:func_packet
:: %1 is version, %2 is suffixname
if "%2" == "" (
set "target=%work_dir%\%publish%\Reader_v%1\%sln_name%_v%1.7z"
) else (
set "target=%work_dir%\%publish%\Reader_v%1\%sln_name%_v%1_%2.7z"
)
cd "%work_dir%\%publish%\Reader_v%1"
..\..\tool\7z a %target% %sln_name%.exe readme.txt
del %sln_name%.exe
del readme.txt
cd "%work_dir%"
goto:eof

:func_cleanup
:: %1 is config, %2 need clean target
if exist "%work_dir%\%sln_name%\%1" rd "%work_dir%\%sln_name%\%1" /q /s
if "%2" == "1" (
if exist "%work_dir%\%1" rd "%work_dir%\%1" /q /s
)
goto:eof
