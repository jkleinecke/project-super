@echo off

REM ctime -begin project_super.ctm

set DebugFlags=-MTd -Oi -Od 
set ReleaseFlags=-MT -Oi -O2 -Zo

REM -analyze
set CompilerDefines=-DPROJECTSUPER_INTERNAL=1 -DPROJECTSUPER_SLOW=1 -DPROJECTSUPER_WIN32=1 -D_CRT_SECURE_NO_WARNINGS=1
REM set CompilerDefines=-DPROJECTSUPER_INTERNAL=0 -DPROJECTSUPER_SLOW=0 -DPROJECTSUPER_WIN32=1

set CompilerFlags=%CompilerDefines% %DebugFlags% -nologo -GR- -EHa- -WX -W4 -wd4100 -wd4201 -wd4505 -wd4310 -wd4309 -FC -Zi 
REM set CompilerFlags=%ReleaseFlags% -nologo -GR- -EHa- -WX -W4 -wd4100 -wd4201 -FC -Zi 

set LinkerFlags=-incremental:no -opt:ref

set GameCompilerFlags=%CompilerDefines% %CompilerFlags% /LD 
set GameLinkerFlags=%LinkerFlags% -PDB:ps_game_%random%.pdb /DLL /EXPORT:GameUpdateAndRender -out:ps_game.dll

set HostCompilerFlags=%CompilerDefines% %CompilerFlags%
set HostLinkerFlags=%LinkerFlags% user32.lib gdi32.lib Ole32.lib winmm.lib -out:project_super.exe

IF NOT EXIST build mkdir build
pushd build
del *.pdb > NUL 2> NUL
cl %GameCompilerFlags% ..\src\ps_platform.cpp /link %GameLinkerFlags%
cl %HostCompilerFlags% ..\src\win32\win32_platform.cpp /link %HostLinkerFlags%
set LastError=%ERRORLEVEL%

popd

REM ctime -end project_super.ctm %LastError%
