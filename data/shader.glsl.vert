#version 460

// layout(binding = 0) uniform FrameObject {
//     mat4 viewProjection;
// } frame;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 fragTexCoord;

layout( push_constant ) uniform constants
{
	mat4 mvp;
} PushConstants;

void main() {
    // gl_Position = vec4(inPosition, 1.0);
    gl_Position = PushConstants.mvp * vec4(inPosition, 1.0);
    fragNormal = inNormal;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}

