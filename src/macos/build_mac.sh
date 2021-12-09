#!/bin/bash

CompilerFlags="-fno-exceptions -fno-rtti  -g -std=c++20 -Wall -Wno-format -Wno-switch -Wno-write-strings -Wno-multichar -Wno-unused-function -Wno-unused-variable -Wno-missing-braces -Wno-unused-value"
CompilerDefines="-DPROJECTSUPER_INTERNAL=1 -DPROJECTSUPER_SLOW=1 -DPROJECTSUPER_MACOS=1"

LinkerFlags="-lstdc++ -framework Cocoa -framework IOKit -framework AudioUnit"

if [ ! -d "./build/" ]; then
    mkdir build
fi

pushd build
clang++ $CompilerFlags $CompilerDefines -I../src -lstdc++ -dynamiclib ../src/ps_game.cpp -o project_super.A.dylib
clang++ $CompilerFlags $CompilerDefines $LinkerFlags -lvulkan -I../src ../src/macos/macos_platform.mm -o project_super 
popd