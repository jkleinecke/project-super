


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
