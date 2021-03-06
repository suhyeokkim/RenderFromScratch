@echo off

set BUILDER_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBUILD.EXE"
set BUILDER_OPTION=/m
set SUBMODULE_UPDATE=git submodule update --init
set DIRECTXTEX_SLN=./DirectXTex.sln
set DIRECTXTEX_PATH=./DirectXTex
set EASTL_SLN=./EASTL.sln
set EASTL_PATH=./EASTL

set OUT_VAR=' '
echo ::%SUBMODULE_UPDATE%
call %SUBMODULE_UPDATE%

if  %ERRORLEVEL% NEQ 0 GOTO :error 
echo. 

if exist %DIRECTXTEX_PATH% (
	echo ::cd %DIRECTXTEX_PATH%
	cd %DIRECTXTEX_PATH%
	echo ::cmake -A x64 ./
	call cmake -A x64 ./
	echo ::%BUILDER_PATH% %DIRECTXTEX_SLN% %BUILDER_OPTION% /p:configuration=Debug
	call %BUILDER_PATH% %DIRECTXTEX_SLN% %BUILDER_OPTION% /p:configuration=Debug
	echo ::%BUILDER_PATH% %DIRECTXTEX_SLN% %BUILDER_OPTION% /p:configuration=Release
	call %BUILDER_PATH% %DIRECTXTEX_SLN% %BUILDER_OPTION% /p:configuration=Release
	cd ../
) else (
	echo ::DirectXTex not exist..
)
echo. 

if exist %EASTL_PATH% (
	echo ::cd %EASTL_PATH%
	cd %EASTL_PATH%
	echo ::cmake -A x64 ./
	call cmake -A x64 ./
	echo ::%BUILDER_PATH% %EASTL_SLN% %BUILDER_OPTION% /p:configuration=Debug
	call %BUILDER_PATH% %EASTL_SLN% %BUILDER_OPTION% /p:configuration=Debug
	echo ::%BUILDER_PATH% %EASTL_SLN% %BUILDER_OPTION% /p:configuration=Release
	call %BUILDER_PATH% %EASTL_SLN% %BUILDER_OPTION% /p:configuration=Release
	cd ../
) else (
	echo ::EASTL not exist..
)
echo. 

echo ::ALL LIB SETUP END

:error
pause