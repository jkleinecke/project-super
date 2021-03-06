#version 460

struct LightData
{
    vec3 pos;
    vec3 color;
};

struct Material
{
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
};

// Per Scene
layout(std140, set = 0, binding = 0) uniform Scene {
    mat4 viewProj;
    vec3 cameraPos;
    LightData light;
} scene;

// // Per Material
// layout(std140, set = 1, binding = 0) uniform MaterialData {
//     Material data[49]; 
// } materials;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMap;
// layout(set = 2, binding = 4) uniform sampler2D aoMap;


layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec2 inTexCoords;
// layout(location = 3) flat in uint inMaterialIndex;

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

// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(normalMap, inTexCoords).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(inWorldPos);
    vec3 Q2  = dFdy(inWorldPos);
    vec2 st1 = dFdx(inTexCoords);
    vec2 st2 = dFdy(inTexCoords);

    vec3 N   = normalize(inNormal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main()
{
    vec3 albedo     = texture(albedoMap, inTexCoords).rgb;
    vec3 normal     = getNormalFromMap();
    float metallic  = texture(metallicMap, inTexCoords).r;
    float roughness = texture(roughnessMap, inTexCoords).r;
    float ao        = 1.0f; //texture(aoMap, inTexCoords).r;

    vec3 N = normalize(normal);
    vec3 V = normalize(scene.cameraPos - inWorldPos);

    // NOTE(james): PBR metallic workflow let's us assume dailectric surface uses a constant of 0.4
    vec3 F0 = vec3(0.04);
    F0      = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);    // light-out
    /////
    // for each light (we only have one right now)   
    /////

    vec3 L = normalize(scene.light.pos - inWorldPos);
    vec3 H = normalize(V + L);
    float distance = length(scene.light.pos - inWorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = scene.light.color * attenuation;

    // NOTE(james): Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);        
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    //////////////////
    // end light calc
    //////////////////

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    //-- Tone-map
    color = color / (color + vec3(1.0));
    //color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color, 1.0);
}