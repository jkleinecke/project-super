#version 460


struct LightData
{
    vec3 pos;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// Per Scene
layout(set = 0, binding = 0) uniform Scene {
    mat4 viewProj;
    uniform vec3 cameraPos;
    uniform LightData light;
} scene;

// Per Material
layout(set = 1, binding = 0) uniform Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
} material;

// Per Instance
// layout(set = 2, binding = 0) uniform PerInstance {
//     uint material;
// } Instance;

// TODO(james): Need to pass the fragment position (via the world matrix) from the vertex shader
//              This means that I need to spend some time setting up a true material system
//              for more easily passing data into the shaders.

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inPosition;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outFragPos;

layout( push_constant ) uniform constants
{ 
    mat4 world;
} PushConstants;

void main()
{ 
    gl_Position = scene.viewProj * PushConstants.world * vec4(inPosition, 1.0);
    // TODO(james): pass the "normal matrix" into the shader via a uniform rather than
    //              calculating on every vertex
    outNormal = mat3(transpose(inverse(PushConstants.world))) * inNormal;     // transforming the normal into world space
    //outNormal = inNormal;     // transforming the normal into world space
    outFragPos = vec3(PushConstants.world * vec4(inPosition, 1.0));
}