#!/bin/bash

mkdir build
pushd build
clang++ ../src/macos_platform_main.mm -o macos_platform_main
popd