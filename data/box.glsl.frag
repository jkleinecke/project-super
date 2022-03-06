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

// const vec3 viewPos = vec3(1.0, 2.0, 5.0);
// const vec3 lightPos = vec3(1.2, 1.0, 2.0);

// const vec3 objectColor = vec3(1.0, 0.5, 0.31);
// const vec3 lightColor = vec3(1.0, 1.0, 1.0);

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inFragPos;

layout(location = 0) out vec4 outColor;

void main()
{
    // Phong Lighting Model

    // Ambient Term

    vec3 ambient = scene.light.ambient * material.ambient;

    // Diffuse Term

    vec3 norm = normalize(inNormal);
    vec3 lightDir = normalize(scene.light.pos - inFragPos);
    vec3 viewDir = normalize(scene.cameraPos - inFragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    float diff = max(dot(lightDir, norm), 0.0);
    vec3 diffuse = scene.light.diffuse * (diff * material.diffuse);

    // Specular Term

    float spec = pow(max(dot(norm, halfwayDir), 0.0), material.shininess);
    vec3 specular = scene.light.specular * (spec * material.specular);

    // Now apply all the lighting

    vec3 result = (ambient + diffuse + specular);

    outColor = vec4(result, 1.0);
}