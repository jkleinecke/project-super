@echo off

REM ctime -begin project_super.ctm

set CompilerFlags=-nologo -GR- -EHa- -Od -Oi -WX -W4 -wd4100 -wd4201 -FC -Zi -analyze
set CompilerDefines=-DPROJECTSUPER_INTERNAL=1 -DPROJECTSUPER_SLOW=1 -DPROJECTSUPER_WIN32=1
set LinkerFlags=user32.lib gdi32.lib Ole32.lib

IF NOT EXIST build mkdir build
pushd build

cl %CompilerDefines% %CompilerFlags% ..\src\win32_platform_main.cpp %LinkerFlags%
set LastError=%ERRORLEVEL%

popd

REM ctime -end project_super.ctm %LastError%
