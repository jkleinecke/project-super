#version 460 core

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

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(scene.light.color, 1.0);
}