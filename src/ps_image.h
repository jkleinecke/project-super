#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb/stb_image.h"

#if 0
// TODO(james): Move this into asset management, or maybe turn this file into asset management
internal b32
LoadRenderImage(memory_arena& arena, const char* filename, render_image& image)
{
    platform_file file = Platform.OpenFile(FileLocation::Content, filename, FileUsage::Read);
    // TODO(james): replace malloc with temporary asset buffer
    u8* fileBytes = malloc(file.size);
    Platform.ReadFile(file, fileBytes, file.size);

    b32 success = false;
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load_from_memory(fileBytes, file.size, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    ASSERT(pixels);

    if(pixels)
    {
        image.width = texWidth;
        image.height = texHeight;
        ASSERT(texChannels == 4);
        image.format = RenderImageFormat::RGBA;
        image.pixels = PushArray(arena, sizeof(u32), texWidth*texHeight);

        Copy(texWidth*texHeight*texChannels, pixels, image.pixels);

        success = true;
    }
    // TODO(james): replace with temporary asset buffer
    stbi_image_free(pixels);
    free(fileBytes);

    return success;
}
#endif