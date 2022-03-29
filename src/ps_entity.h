
#include <flecs/flecs.h>

struct Position
{
    v3 position;
};

struct Velocity
{
    v3 velocity;
};

struct Mesh
{
    u32 indexCount;
    u32 baseVertex;
    u32 baseIndex;
    GfxBuffer indexBuffer;
    GfxBuffer vertexBuffer;  // For now we only need 1
};

struct Material
{
    GfxProgram shaders;
    GfxKernel kernel;

    GfxTexture albedo;
    GfxTexture normal;
    GfxTexture metallic;
    GfxTexture roughness;
};