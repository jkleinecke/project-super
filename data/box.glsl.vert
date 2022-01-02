#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inPosition;

layout( push_constant ) uniform constants
{ 
    mat4 mvp;
} PushConstants;

void main()
{ 
    gl_Position = PushConstants.mvp * vec4(inPosition, 1.0);
}