rem **Download sources and binaries of libraries**

cd /D "%~dp0"

rem **SVN revision to checkout for windows-libs**
rem **Update this line when you commit an update to windows-libs**
set "svnrev=28256"

svn co https://svn.wildfiregames.com/public/windows-libs/trunk@%svnrev% win32 || exit /b 1

rem **Copy dependencies' binaries to binaries/system/**

rem static libs: boost fmt
rem wxwidgets isn't provided and needs to be built manually
set DIR_LIST=enet fcollada freetype gloox iconv icu libcurl libpng libsodium libxml2 microsoft miniupnpc nvtt openal sdl2 spidermonkey vorbis zlib
for %%d in (%DIR_LIST%) do (
    copy /y win32\%%d\bin\* ..\binaries\system\ || exit /b 1
)

rem **Copy build tools to build/bin

if exist ..\build\bin\ rmdir ..\build\bin\ /s /q || exit /b 1
mkdir ..\build\bin\ || exit /b 1

set TOOLCHAIN_DIR_LIST=premake-core cxxtest-4.4
for %%d in (%TOOLCHAIN_DIR_LIST%) do (
    copy /y win32\%%d\bin\* ..\build\bin\ || exit /b 1
)
