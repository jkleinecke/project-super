@echo off

REM Generated from 4coder project initialization
REM set opts=-FC -GR- -EHa- -nologo -Zi
REM set code=%cd%
REM pushd build
REM cl %opts% %code%\src\win32\build.bat -Febuild
REM popd
REM ====

REM ctime -begin project_super.ctm

pushd data
echo Building Shaders...
Powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '.\build.ps1'"
popd

REM Setup the build directory
IF NOT EXIST build mkdir build

REM copy the redist files
REM echo Copying redistributable files...
REM xcopy redist\win32 build /D /E /I > NUL


REM Use the build directory
pushd build
del *.pdb > NUL 2> NUL

set DebugFlags=-Oi -Od 
set ReleaseFlags=-Oi -O2 -Zo

REM set CommonCompilerFlags=-diagnostics:column -WL -O2 -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4127 -FC -Z7 -GS- -Gs9999999

REM -analyze
set CompilerDefines=-DPROJECTSUPER_INTERNAL=1 -DPROJECTSUPER_SLOW=1 -DPROJECTSUPER_WIN32=1 
REM set CompilerDefines=-DPROJECTSUPER_INTERNAL=1 -DPROJECTSUPER_WIN32=1

REM turn warning 4350 back on once the renderer backend is split from the host
set CompilerFlags=%CompilerDefines% %DebugFlags% -WL -nologo /std:c++20 -fp:fast -fp:except- -GS- -GR- -EHa- -WX -W4 -wd4834 -wd4701 -wd4706 -wd4100 -wd4530 -wd4201 -wd4505 -wd4189 -wd4324 -wd4244 -wd4127 -wd4702 -FC -Zi 

REM add /FUNCTIONPADMIN for vs2022 hotpatching??
set LinkerFlags=-incremental:no -opt:ref

set GameCompilerFlags=-MTd -D_CRT_SECURE_NO_WARNINGS %CompilerDefines% %CompilerFlags% /LD 
set GameLinkerFlags=%LinkerFlags% -PDB:ps_game_%random%.pdb /DLL /EXPORT:GameUpdateAndRender -out:ps_game.dll

REM TODO: Split renderer into a seperate DLL where we can have more than one API backend, like opengl32.lib

set OpenGLLibs=opengl32.lib
set D3d12Libs=D3d12.lib d3dcompiler.lib DXGI.lib
set VulkanLibs=vulkan-1.lib
REM set VulkanSdkDir=C:\\VulkanSDK\\1.2.198.0
set VulkanIncludeDir=%VULKAN_SDK%\\Include
set VulkanLibDir=%VULKAN_SDK%\\Lib

set GraphicsLibs=%VulkanLibs%

set HostCompilerFlags=%CompilerDefines% %CompilerFlags% 
REM Removed the CRT
REM set HostLinkerFlags=/NODEFAULTLIB /SUBSYSTEM:windows -STACK:0x100000,0x100000 %LinkerFlags% kernel32.lib ole32.lib uuid.lib user32.lib gdi32.lib winmm.lib %GraphicsLibs% -out:project_super.exe
set HostLinkerFlags=-STACK:0x100000,0x100000 %LinkerFlags% ole32.lib user32.lib gdi32.lib winmm.lib %GraphicsLibs% -out:project_super.exe


REM clang++ %CompilerDefines% -I..\src -Oi -Od -std=c++17 -I%VulkanIncludeDir% ..\src\win32\win32_platform.cpp  

cl %GameCompilerFlags% -I%VulkanIncludeDir% -I..\src\libs ..\src\ps_game.cpp ..\src\libs\flecs\flecs.c ..\src\libs\tinyobjloader\tiny_obj_loader.cc -Fmps_game.map /link %GameLinkerFlags%
cl %HostCompilerFlags% -I..\src -I..\src\libs -I%VulkanIncludeDir% ..\src\win32\win32_platform.cpp ..\src\vulkan\vma.cpp ..\src\libs\SPIRV-Reflect\spirv_reflect.c -Fmwin32_platform.map /link -LIBPATH:%VulkanLibDir% %HostLinkerFlags%
set LastError=%ERRORLEVEL%

REM pop build directory
popd

REM ctime -end project_super.ctm %LastError%

