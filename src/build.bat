@echo off

REM ctime -begin project_super.ctm

set CompilerFlags=-nologo -GR- -EHa- -Od -Oi -WX -W4 -wd4100 -wd4201 -FC -Zi 
REM -analyze
set CompilerDefines=-DPROJECTSUPER_INTERNAL=1 -DPROJECTSUPER_SLOW=1 -DPROJECTSUPER_WIN32=1
set LinkerFlags=user32.lib gdi32.lib Ole32.lib winmm.lib

IF NOT EXIST build mkdir build
pushd build
del *.pdb > NUL 2> NUL
del temp_project_super.dll > NUL 2> NUL 
cl %CompilerDefines% %CompilerFlags% ..\src\project_super.cpp /LD /link -incremental:no -opt:ref -PDB:project_super_%random%.pdb /DLL /EXPORT:GameUpdateAndRender
cl %CompilerDefines% %CompilerFlags% ..\src\win32_platform_main.cpp /link -incremental:no %LinkerFlags%
set LastError=%ERRORLEVEL%

popd

REM ctime -end project_super.ctm %LastError%
