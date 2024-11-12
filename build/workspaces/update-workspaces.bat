@echo off
rem ** Create Visual Studio Workspaces on Windows **

cd ..\bin
if not exist ..\workspaces\vs2017\SKIP_PREMAKE_HERE premake5.exe --file="../premake/premake5.lua" --outpath="../workspaces/vs2017" %* vs2017
cd ..\workspaces
