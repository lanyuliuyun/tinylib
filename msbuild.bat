@echo off

set target=Build
set config=Debug

if /i "%1"=="c" (
	set target=Clean
)
if /i "%1"=="r" (
	set target=Rebuild
)
if /i "%2"=="r" (
	set config=Release
)

msbuild.exe /m /nologo /t:%target% /p:Configuration=%config% vsprj/tinylib.vcxproj
