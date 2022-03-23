
#include <flecs/flecs.h>

struct TransformNode
{
    v3 position;
    q4 rotation;
    v3 scale;
};

struct Velocity
{
    v3 velocity;
};
