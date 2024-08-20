rem **Download sources and binaries of libraries**

rem **SVN revision to checkout for source-libs and windows-libs**
rem **Update this line when you commit an update to source-libs or windows-libs**
set "svnrev=28207"

if exist source\.svn (
  cd source && svn cleanup && svn up -r %svnrev% && cd ..
) else (
  svn co -r %svnrev% https://svn.wildfiregames.com/public/source-libs/trunk source
)

if exist win32\.svn (
  cd win32 && svn cleanup && svn up -r %svnrev% && cd ..
) else (
  svn co -r %svnrev% https://svn.wildfiregames.com/public/windows-libs/trunk win32
)

rem **Copy binaries to binaries/system/**

copy source\fcollada\bin\* ..\binaries\system\
copy source\nvtt\bin\* ..\binaries\system\
copy source\spidermonkey\bin\* ..\binaries\system\
for /d %%l in (win32\*) do (if exist %%l\bin copy /y %%l\bin\* ..\binaries\system\)
