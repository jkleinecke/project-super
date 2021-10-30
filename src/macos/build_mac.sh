#!/bin/bash

CompilerFlags="-fno-exceptions -fno-rtti  -g -std=c++11 -Wall -Wno-format -Wno-switch -Wno-write-strings -Wno-multichar -Wno-unused-function -Wno-unused-variable -Wno-missing-braces"
CompilerDefines="-DPROJECTSUPER_INTERNAL=1 -DPROJECTSUPER_SLOW=1 -DPROJECTSUPER_MACOS=1"

LinkerFlags="-lstdc++ -framework Cocoa -framework IOKit -framework AudioUnit"

echo $CompilerFlags
mkdir build
pushd build
clang++ $CompilerFlags $CompilerDefines $LinkerFlags ../src/macos/macos_platform_main.mm -o project_super 
popd