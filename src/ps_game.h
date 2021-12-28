

#include "ps_platform.h"
// config?
#include "ps_intrinsics.h"
#include "ps_math.h"
#include "ps_shared.h"
#include "ps_memory.h"
#include "ps_stream.h"
#include "ps_image.h"

#include <vulkan/vulkan.h>

#include "vulkan/vk_device.h"   // TODO(james): remove when ps_graphics.h can stand on its own
#include "ps_graphics.h"
#include "ps_render.h"


struct game_state
{
    m4 cameraProjection;
    camera camera;
    render_geometry mesh;
    render_image image;
};