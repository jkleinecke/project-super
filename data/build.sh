#!/bin/bash

# glslc shader.glsl.vert -o vert.spv
# glslc shader.glsl.frag -o frag.spv

FILES="*.glsl.vert
*.glsl.frag"
for shader in $FILES;
do
    output="${shader/.glsl/}.spv"

    if [[ $shader -nt $output ]]; then
        echo "Compiling modified shader file $shader"
        glslc --target-env=vulkan1.1 -I./ $shader -o $output

        if [[ $? -eq 0 ]]; then
            echo "Successfully compiled $output"
        else
            echo "Error! $shader failed to validate"
        fi
    fi
done