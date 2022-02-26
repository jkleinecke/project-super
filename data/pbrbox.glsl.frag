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

// Per Material
layout(std140, set = 1, binding = 0) uniform Material {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
} material;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inFragPos;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

void main()
{
    vec3 N = normalize(inNormal);
    vec3 V = normalize(scene.cameraPos - inFragPos);

    vec3 F0 = vec3(0.4);
    F0      = mix(F0, material.albedo, material.metallic);

    vec3 Lo = vec3(0.0);    // light-out
    /////
    // for each light (we only have one right now)   
    /////

    vec3 L = normalize(scene.light.pos - inFragPos);
    vec3 H = normalize(V + L);
    float distance = length(scene.light.pos - inFragPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = scene.light.color * attenuation;

    // NOTE(james): PBR metallic workflow let's us assume dailectric surface uses a constant of 0.4

    vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

    float NDF = DistributionGGX(N, H, material.roughness);
    float G = GeometrySmith(N, V, L, material.roughness);

    // NOTE(james): Cook-Torrance BRDF
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - material.metallic;

    float NdotL = max(dot(N, L), 0.0);        
    Lo += (kD * material.albedo / PI + specular) * radiance * NdotL;
    //////////////////
    // end light calc
    //////////////////

    vec3 ambient = vec3(0.03) * material.albedo * material.ao;
    vec3 color = ambient + Lo;

    //-- Tone-map
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color, 1.0);
}