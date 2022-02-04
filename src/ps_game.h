

#include "ps_platform.h"
// config?
#include "ps_intrinsics.h"
#include "ps_math.h"
#include "ps_shared.h"
#include "ps_memory.h"
#include "ps_collections.h"
#include "ps_stream.h"
#include "ps_image.h"
// #include "ps_graphics.h"
#include "ps_render.h"
#include "ps_asset.h"

struct renderable
{
    render_material material;
    render_geometry geometry;
};

struct game_state
{
    memory_arena totalArena;
    memory_arena* frameArena;
    temporary_memory temporaryFrameMemory;

    game_assets* assets;
    // render_resource_queue* resourceQueue;

    GfxCmdEncoderPool cmdpool;
    GfxCmdContext cmds;
    GfxProgram shaderProgram;
    GfxKernel mainKernel;
    renderable box;

    m4 cameraProjection;
    camera camera;

    v3 lightPosition;
    f32 lightScale;

    v3 position;
    f32 scaleFactor;
    f32 rotationAngle;
};