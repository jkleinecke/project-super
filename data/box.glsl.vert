#version 460

#include "Common.glsl"

layout(set = 0, binding = 0) uniform SceneBlock {
    WindowData window;
    FrameData frame;
    CameraData camera;
} Scene;

//--TODO: Figure out how to wait for upload of instance buffer
layout(set = 1, binding = 0) uniform InstanceBlock {
    mat4 mvp;
    mat4 world;
    mat4 worldNormal;
    uint material;
} Instance;

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
    gl_Position = Scene.camera.viewProj * PushConstants.world * vec4(inPosition, 1.0);
    // TODO(james): pass the "normal matrix" into the shader via a uniform rather than
    //              calculating on every vertex
    outNormal = mat3(transpose(inverse(PushConstants.world))) * inNormal;     // transforming the normal into world space
    //outNormal = mat3(Instance.worldNormal) * inNormal;     // transforming the normal into world space
    outFragPos = vec3(PushConstants.world * vec4(inPosition, 1.0));
}