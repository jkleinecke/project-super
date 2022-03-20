#version 460


struct LightData
{
    vec3 pos;
    vec3 color;
};

// Per Scene
layout(std140, set = 0, binding = 0) uniform Scene {
    mat4 viewProj;
    vec3 cameraPos;
    LightData light;
} scene;

// Per Instance
// layout(set = 2, binding = 0) uniform PerInstance {
//     uint material;
// } Instance;

// TODO(james): Need to pass the fragment position (via the world matrix) from the vertex shader
//              This means that I need to spend some time setting up a true material system
//              for more easily passing data into the shaders.

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inPosition;
layout(location = 2) in vec2 inTexCoords;

layout( push_constant ) uniform constants
{ 
    mat4 world;
} PushConstants;

void main()
{ 
    gl_Position = scene.viewProj * PushConstants.world * vec4(inPosition, 1.0);
}