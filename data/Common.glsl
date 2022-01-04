
struct WindowData
{
    float posX;
    float posY;
    float width;
    float height;
};

struct FrameData
{
    float time;
    float timeDelta;
};

struct CameraData
{
    vec3 pos;
    mat4 view;
    mat4 proj;
    mat4 viewProj;
};

struct LightData
{
    vec3 pos;
    vec3 color;
};

struct MaterialData
{
    vec3 color;
};

struct ObjectData
{
    mat4 mvp;
    mat4 world;
    mat4 worldNormal;
    uint material;   
};
