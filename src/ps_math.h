
// turn off the handmade math prefix to simplify code
#define HMM_PREFIX(name) name

//#define HMM_SINF MySinF
//#define HMM_COSF MyCosF
//#define HMM_TANF MyTanF
#define HMM_SQRTF SquareRoot
//#define HMM_EXPF MyExpF
//#define HMM_LOGF MyLogF
//#define HMM_ACOSF MyACosF
//#define HMM_ATANF MyATanF
//#define HMM_ATAN2F ATan2

#include "hmm_math.h"

typedef hmm_v2 v2;
typedef hmm_v3 v3;
typedef hmm_v4 v4;
typedef hmm_m4 m4;
typedef hmm_quaternion quaternion;

// GLM Based Perspective Calc
// Maps depth range from 0..1
// Should also correct the flipped Y issue
HMM_INLINE m4 Perspective(f32 FOV, f32 width, f32 height, float Near, float Far)
{
    m4 Result = Mat4();

    f32 rad = FOV * (HMM_PI32 / 360.0f);
    f32 h = CosF(0.5f * rad) / SinF(0.5f * rad);
    f32 w = h * height / width; // TODO(james): Assumes width is greater than height
    float Cotangent = 1.0f / HMM_PREFIX(TanF)(FOV * (HMM_PI32 / 360.0f));

    Result.Elements[0][0] = w;
    Result.Elements[1][1] = -h;
    Result.Elements[2][3] = -1.0f;
    Result.Elements[2][2] = Far / (Near - Far);
    Result.Elements[3][2] = -(Far * Near) / (Far - Near);
    Result.Elements[3][3] = 0.0f;

    return (Result);
}