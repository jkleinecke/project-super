#!/bin/bash

CompilerFlags="-fpermissive -fno-exceptions -fno-rtti  -g -std=c++20 -Wall -Wno-format -Wno-switch -Wno-write-strings -Wno-multichar -Wno-unused-function -Wno-unused-variable -Wno-missing-braces -Wno-unused-value -Wno-nullability-completeness -Wno-reorder-ctor -Wno-deprecated -Wno-address-of-temporary"
CompilerDefines="-DPROJECTSUPER_INTERNAL=1 -DPROJECTSUPER_SLOW=1 -DPROJECTSUPER_MACOS=1"

LinkerFlags="-lstdc++ -framework Cocoa -framework IOKit -framework AudioUnit"

pushd data
echo Building Shaders...
./build.sh
popd


if [ ! -d "./build/" ]; then
    mkdir build
fi

pushd build
clang  -c ../src/libs/flecs/flecs.c -o flecs.o
clang++ $CompilerFlags $CompilerDefines -I../src -I../src/libs -lstdc++ -dynamiclib ../src/ps_game.cpp flecs.o ../src/libs/tinyobjloader/tiny_obj_loader.cc -o ps_game.dylib
clang -c ../src/libs/SPIRV-REFLECT/spirv_reflect.c -o spirv_reflect.o
clang++ $CompilerFlags $CompilerDefines $LinkerFlags -lvulkan -I../src -I../src/libs ../src/macos/macos_platform.mm spirv_reflect.o ../src/vulkan/vma.cpp -o project_super 
popd

# {
#             "name": "Mac",
#             "includePath": [
#                 "${default}",
#                 "${workspaceFolder}/src",
#                 "${workspaceFolder}/src/libs",
#                 "/usr/local/include"
#             ],
#             "macFrameworkPath": [
#                 "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks"
#             ],
#             "compilerPath": "/usr/bin/clang",
#             "intelliSenseMode": "clang-x64",
#             "compilerArgs": [
#                 "-fno-exceptions",
#                 "-fno-rtti",
#                 "-g",
#                 "-std=c++20",
#                 "-Wall",
#                 "-Wno-format",
#                 "-Wno-switch",
#                 "-Wno-write-strings",
#                 "-Wno-multichar",
#                 "-Wno-unused-function",
#                 "-Wno-unused-variable",
#                 "-Wno-missing-braces"
#             ],
#             "defines": [
#                 "PROJECTSUPER_INTERNAL=1",
#                 "PROJECTSUPER_SLOW=1",
#                 "PROJECTSUPER_MACOS=1"
#             ],
#             "cStandard": "c17",
#             "cppStandard": "c++20",
#             "browse": {
#                 "limitSymbolsToIncludedHeaders": true
#             }
#         }