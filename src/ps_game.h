

#include "ps_platform.h"
// config?
#include "ps_intrinsics.h"
#include "ps_math.h"
#include "ps_shared.h"
#include "ps_memory.h"
#include "ps_collections.h"
#include "ps_stream.h"
#include "ps_image.h"

#include <flecs/flecs.h>
#include "ps_entity.h"
#include "ps_render.h"
#include "ps_asset.h"

struct game_state
{
    memory_arena totalArena;
    memory_arena* frameArena;
    temporary_memory temporaryFrameMemory;

    ecs_world_t* world;
    // ecs_world_t* stage1;

    game_assets* assets;
    render_context* renderer;

    m4 cameraProjection;
    camera camera;

    v3 lightPosition;
    f32 lightScale;

    v3 position;
    f32 scaleFactor;
    f32 rotationAngle;
};