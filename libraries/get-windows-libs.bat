rem **Download sources and binaries of libraries**

rem **SVN revision to checkout for windows-libs**
rem **Update this line when you commit an update to windows-libs**
set "svnrev=28209"

svn co https://svn.wildfiregames.com/public/windows-libs/trunk@%svnrev% win32

rem **Copy binaries to binaries/system/**

for /d %%l in (win32\*) do (if exist %%l\bin copy /y %%l\bin\* ..\binaries\system\)
