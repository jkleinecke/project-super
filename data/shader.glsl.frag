#version 460

// layout(binding = 0) uniform sampler2D texSampler;

layout(binding = 0) uniform Material {
    vec4 uniColor[];
} material;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = material.uniColor[0];//* vec4(fragColor, 1.0);

    //outColor = texture(texSampler, fragTexCoord);
    
    // outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);
}