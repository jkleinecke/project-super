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



// TODO(james): Need to pass the fragment position (via the world matrix) from the vertex shader
//              This means that I need to spend some time setting up a true material system
//              for more easily passing data into the shaders.

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inPosition;
//layout(location = 2) in vec2 inTexCoords;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outFragPos;
//layout(location = 2) out vec2 outTexCoords;
// layout(location = 3) flat out uint outMaterialIndex;

layout( push_constant ) uniform constants
{ 
    mat4 world;
    // uint materialIndex;
} PushConstants;

void main()
{ 
    gl_Position = scene.viewProj * PushConstants.world * vec4(inPosition, 1.0);
    //outTexCoords = inTexCoords;
    // TODO(james): pass the "normal matrix" into the shader via a uniform rather than
    //              calculating on every vertex
    outNormal = mat3(PushConstants.world) * inNormal;     // transforming the normal into world space
    //outNormal = inNormal;     // transforming the normal into world space
    outFragPos = vec3(PushConstants.world * vec4(inPosition, 1.0));
    // outMaterialIndex = PushConstants.materialIndex;
}