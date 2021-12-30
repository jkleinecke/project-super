

#include "ps_platform.h"
// config?
#include "ps_intrinsics.h"
#include "ps_math.h"
#include "ps_shared.h"
#include "ps_memory.h"
#include "ps_stream.h"
#include "ps_image.h"
#include "ps_render.h"
#include "ps_asset.h"



struct game_state
{
    memory_arena totalArena;
    memory_arena* frameArena;
    temporary_memory temporaryFrameMemory;

    game_assets* assets;
    render_resource_queue* resourceQueue;

    m4 cameraProjection;
    camera camera;
};