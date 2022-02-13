#include "ps_game.h"
#include "ps_audio_synth.cpp"
#include "ps_render.cpp"
#include "ps_asset.cpp"



#if 0
//#define GAME_LOG(msg, ...) Platform.Log(__FILE__, __LINE__, msg, __VA_ARGS__)
#define GAME_LOG

#define TONE_AMPLITUDE 1000

struct SoundTone
{
    u32 toneIndex;
    b32 isActive;
    u32 playbackSampleIndex;
    
    // pre-compute the tone samples on initialization..
    u32 sample_count;
    nf32* tone_samples;
};

struct GameTestState
{
    int x_offset;
    int y_offset;
    
    SoundTone tones[6];
    
    i16 lastSecondAudioSamples[48000 * 2];
    u32 indexIntoLastSecondSamples;
};

internal void RenderVerticalLine(const GraphicsContext& graphics, u32 x, u32 y1, u32 y2, u32 color)
{
    // normalize so that y1 is always < than y2
    if(y1 > y2)
    {
        int t = y1;
        y1 = y2;
        y2 = t;
    }
    
    IFF(y2 >= graphics.buffer_height, y2 = graphics.buffer_height-1);
    
    u32* pixels = (u32*)graphics.buffer;
    
    for(u32 row = y1; row <= y2; ++row)
    {
        pixels[(row * graphics.buffer_width) + x] = color;
    }
}

internal void RenderHorizontalLine(const GraphicsContext& graphics, u32 x1, u32 x2, u32 y, u32 color)
{
    // normalize so that x1 is always < x2
    if(x1 > x2)
    {
        int t = x1;
        x1 = x2;
        x2 = t;
    }
    
    IFF(x2 >= graphics.buffer_width, x2 = graphics.buffer_width-1);
    
    u32* pixels = (u32*)graphics.buffer;
    for(u32 col = x1; col <= x2; ++col)
    {
        pixels[(y * graphics.buffer_width) + col] = color;
    }
}

internal void
RenderAudioWave(const GraphicsContext& graphics, GameTestState& gameState)
{
    ZeroArray(graphics.buffer_height * graphics.buffer_pitch, (u8*)graphics.buffer);
    
    // now render the audio wave
    u32* pixels = (u32*)graphics.buffer;
    i16* audioSamples = gameState.lastSecondAudioSamples; 
    int samplesPerColumn = 48000 / graphics.buffer_width;
    
    int maxY = (int)graphics.buffer_height;
    int midY = (int)graphics.buffer_height / 2;
    
    u32 leftColor = 255;        // blue
    u32 rightColor = 255 << 16; // red
    
    const f32 amplitude = (f32)(TONE_AMPLITUDE * 4);
    
    RenderHorizontalLine(graphics, 0, graphics.buffer_width, midY, 100 << 8);
    RenderVerticalLine(graphics, graphics.buffer_width/2, 0, graphics.buffer_height, 100 << 8);
    
    int lastLeftY = midY;
    int lastRightY = midY;
    for(u32 x = 0; x < graphics.buffer_width; ++x)
    {
        for(int sampleIndex = 0; sampleIndex < samplesPerColumn; ++sampleIndex)
        {
            f32 fLeftSampleValue = (f32)(*audioSamples) / amplitude;
            ++audioSamples;
            f32 fRightSampleValue = (f32)(*audioSamples) / amplitude;
            ++audioSamples;
            
            IFF(fLeftSampleValue > 1.0f, fLeftSampleValue = 1.0f);
            IFF(fLeftSampleValue < -1.0f, fLeftSampleValue = -1.0f);
            IFF(fRightSampleValue > 1.0f, fRightSampleValue = 1.0f);
            IFF(fRightSampleValue < -1.0f, fRightSampleValue = -1.0f);
            
            // now map the -1..1 sample values into the y coordinate
            int ly = midY + (int)(fLeftSampleValue * (midY));
            int ry = midY + (int)(fRightSampleValue * (midY));
            
#if 0
            RenderVerticalLine(graphics, x, lastLeftY, ly, leftColor);
#endif
            RenderVerticalLine(graphics, x, lastRightY, ry, rightColor);
            
            lastLeftY = ly;
            lastRightY = ry;
        }
    }
}

internal void
RenderGradient(const GraphicsContext& graphics, GameTestState& gameState)
{
    uint8* row = (uint8*)graphics.buffer;
    for(uint32 y = 0; y < graphics.buffer_height; ++y)
    {
        uint32* pixel = (uint32*)row;
        for(uint32 x = 0; x < graphics.buffer_width; ++x)
        {
            uint8 green = (uint8)(y + gameState.y_offset);
            uint8 blue = (uint8)(x + gameState.x_offset);
            
            *pixel++ = (green << 8) | blue;
        }
        
        row += graphics.buffer_pitch;   
    }
}

internal void
FillSoundBuffer(AudioContext& audio, GameTestState& gameState, FrameContext& frame)
{
    const AudioContextDesc& desc = audio.descriptor;
    u32 numRequestedSamples = audio.samplesRequested;
    void* transientMemoryPosition = frame.transientMemory.freePointer;
    // we'll accumulate the samples in floating point
    f32* toneSamples = (f32*)PushArray(frame.transientMemory, sizeof(f32), numRequestedSamples);
    // clear the samples to 0
    ZeroArray(numRequestedSamples, toneSamples);
    
    // all sounds are treated as mono sounds for now
    // TODO(james): Handle stereo
    for(u32 sampleIndex = 0; sampleIndex < numRequestedSamples; ++sampleIndex)
    {
        for(int soundToneIndex = 0; soundToneIndex < 6; ++soundToneIndex)
        {
            SoundTone& tone = gameState.tones[soundToneIndex];
            if(tone.isActive)
            {
                toneSamples[sampleIndex] += tone.tone_samples[tone.playbackSampleIndex++];
                IFF(tone.playbackSampleIndex >= tone.sample_count, tone.playbackSampleIndex = 0);
            }
        }        
    }   
    
    // Now we'll take the accumulated samples and convert to 16-bit and add in the volume
    
    uint16 toneVolume = TONE_AMPLITUDE;
    
    // convert the timestep to the number of samples we need to render
    //   samples = 2 channels of 16 bit audio
    
    //   LEFT RIGHT | LEFT RIGHT | LEFT RIGHT | ...
    
    audio.samplesWritten = numRequestedSamples;    
    
    // copy data to buffer
    int16 *SampleOut = (int16*)audio.streamBuffer.data;
    for(uint32 sampleIndex = 0;
        sampleIndex < numRequestedSamples;
        ++sampleIndex)
    {
        int16 SampleValue = (int16)(toneSamples[sampleIndex] * toneVolume);
        // TODO(james): Handle stereo
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
    }
    
    // reset our scratch memory
    frame.transientMemory.freePointer = transientMemoryPosition;
}

internal inline
void ToggleSoundTone(SoundTone& tone, InputButton& button)
{
    if(button.transitions > 0)
    {
        tone.isActive = button.pressed;
        tone.playbackSampleIndex = 0;
        
        GAME_LOG("Sound tone %d is %s", tone.toneIndex, tone.isActive ? "ACTIVE" : "INACTIVE");
    }
}

// UBOs have alignment requirements depending on the data being uploaded
// so it is always good to be specific about the alignment for a UBO
struct UniformBufferObject  // TODO(james): this is only temporary...
{
    alignas(16) m4 model;
    alignas(16) m4 view;
    alignas(16) m4 proj;
};

internal inline
void RenderScene(GraphicsContext& graphics, GameTestState& gameState, FrameContext& frame)
{
    VkExtent2D extent = graphics.device->extent;    // TODO
    
    // update the g_UBO with the latest matrices
    {
        local_persist f32 accumlated_elapsedFrameTime = 0.0f;
        
        //accumlated_elapsedFrameTime += clock.elapsedFrameTime;
        
        u32 width = extent.width;
        u32 height = extent.height;
        
        UniformBufferObject ubo{};
        // Rotates 90 degrees a second
        ubo.model = Rotate(accumlated_elapsedFrameTime * 90.0f, Vec3(0.0f, 0.0f, 1.0f));
        ubo.view = LookAt(Vec3(4.0f, 4.0f, 4.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = Perspective(45.0f, (f32)width, (f32)height, 0.1f, 10.0f);
        //ubo.proj.Elements[1][1] *= -1;
        
        vg_buffer& uboBuffer = graphics.device->pCurFrame->camera_buffer;   // TODO
        graphics.api->UpdateBufferData(graphics.device, uboBuffer, sizeof(ubo), &ubo);
    }
    
    VkDescriptorSetLayout descriptorLayout = graphics.device->pipeline.descriptorLayout;   // TODO
    VkDescriptorSet shaderDescriptor = graphics.api->CreateDescriptor(graphics.device, descriptorLayout);
    
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = graphics.device->pCurFrame->camera_buffer.handle;   // TODO
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);
    
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = graphics.device->texture.view;  // TODO
    imageInfo.sampler = graphics.device->sampler.handle; // TODO
    
    VkWriteDescriptorSet writes[] = {
        vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderDescriptor, &bufferInfo, 0),
        vkInit_write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderDescriptor, &imageInfo, 1)
    };
    
    graphics.api->UpdateDescriptorSets(graphics.device, ARRAY_COUNT(writes), writes);
    
    VkCommandBuffer cmds = graphics.api->GetCmdBuffer(graphics.device);
    VkFramebuffer framebuffer = graphics.api->GetFramebuffer(graphics.device);
    
    graphics.api->BeginRecordingCmds(cmds);
    
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    VkRenderPass pass = graphics.device->renderPass.handle;    // TODO
    graphics.api->BeginRenderPass(cmds, pass, ARRAY_COUNT(clearValues), clearValues, extent, framebuffer);
    
    VkPipeline pipeline = graphics.device->pipeline.handle; // TODO
    graphics.api->BindPipeline(cmds, pipeline);
    
    VkPipelineLayout pipelineLayout = graphics.device->pipeline.layout; // TODO
    graphics.api->BindDescriptorSets(cmds, pipelineLayout, 1, &shaderDescriptor);
    
    VkBuffer vertexBuffer = graphics.device->vertex_buffer.handle;  // TODO
    VkDeviceSize offsets[] = {0};
    graphics.api->BindVertexBuffers(cmds, 1, &vertexBuffer, offsets);
    
    VkBuffer indexBuffer = graphics.device->index_buffer.handle;    // TODO
    graphics.api->BindIndexBuffer(cmds, indexBuffer);
    
    u32 count = 11484;  // TODO: comes from model
    graphics.api->DrawIndexed(cmds, count, 1);
    
    graphics.api->EndRenderPass(cmds);
    graphics.api->EndRecordingCmds(cmds);
}

#endif

#if 0
internal void
RenderModel(render_commands& cmds, model_asset* model, u32 materialId, m4& viewProj, m4& world, m4& scale)
{
    m4 worldNormal = world;
    m4 scaled = world * scale;
    m4 mvp = viewProj * scaled;

    render_geometry mesh;
    mesh.indexCount = model->indexCount;
    mesh.indexBuffer = model->index_id;
    mesh.vertexBuffer = model->vertex_id;

    // render_material_binding bindings[1];
    // bindings[0].type                = RenderMaterialBindingType::Image;
    // bindings[0].layoutIndex         = 0;        // TODO(james): perhaps change this to a "named" layout?
    // bindings[0].bindingIndex        = 0;
    // bindings[0].image_id            = model->texture_id;
    // bindings[0].image_sampler_index = 0;    // TODO(james): This needs to be easier to figure out that just "knowing" which index to use

    // u32 bindingCount = 0;;

    // if(model->texture_id)
    // {
    //     bindingCount = ARRAY_COUNT(bindings);
    // }

    PushCmd_DrawObject(cmds, mesh, scaled, scaled, worldNormal, materialId);
}

internal
void BuildRenderCommands(game_state& state, render_context& render, const GameClock& clock)
{
    render_commands& cmds = render.commands;

    BeginRenderInfo renderInfo{};
    renderInfo.viewportPosition = Vec2i(0,0);
    renderInfo.viewportSize = render.renderDimensions;
    renderInfo.time = clock.totalTime;
    renderInfo.timeDelta = clock.elapsedFrameTime;
    renderInfo.cameraPos = state.camera.position;
    renderInfo.cameraView = LookAt(state.camera.position, state.camera.target, V3_Y_UP);
    renderInfo.cameraProj = state.cameraProjection;
    BeginRenderCommands(cmds, renderInfo);
    
    m4 viewProj = state.cameraProjection * renderInfo.cameraView;

    // {
    //     m4 mvp = viewProj * M4_IDENTITY;
    //     RenderModel(cmds, state.assets->vikingModel, mvp);
    // }

    // {
    //     v3 scale = Vec3(state.skullScaleFactor,state.skullScaleFactor,state.skullScaleFactor);
    //     // for each object need to compute the mvp
    //     m4 model = Translate(state.skullPosition) * Rotate(state.skullRotationAngle, Vec3i(0,0,1)) * Scale(scale);//M4_IDENTITY;
    //     m4 mvp = viewProj * model;

    //     RenderModel(cmds, state.assets->skullModel, mvp);
    // }

    // v3 lite_ambient = Vec3(0.2f, 0.2f, 0.2f);
    v3 lite_ambient = Vec3(0.1f, 0.1f, 0.1f);
    // v3 lite_diffuse = Vec3(0.5f, 0.5f, 0.5f);
    v3 lite_diffuse = Vec3(0.3f, 0.3f, 0.3f);
    v3 lite_spec = Vec3(1.0f, 1.0f, 1.0f);

    PushCmd_UpdateLight(cmds, state.lightPosition, lite_ambient, lite_diffuse, lite_spec);

    // PushCmd_UsePipeline(cmds, state.assets->mapPipelines->get(pl_box)->id);;

    // {
    //     m4 scale = Scale(Vec3(state.scaleFactor,state.scaleFactor,state.scaleFactor));
    //     m4 model = Translate(state.position) * Rotate(state.rotationAngle, Vec3i(0,1,0)) ;
    //     RenderModel(cmds, state.assets->models, state.assets->mapMaterials->get(mat_box)->id, viewProj, model, scale);
    // }

    // PushCmd_UsePipeline(cmds, state.assets->mapPipelines->get(pl_lightbox)->id);

    // {
    //     m4 scale = Scale(Vec3(state.lightScale, state.lightScale, state.lightScale));
    //     m4 model = Translate(state.lightPosition);
    //     RenderModel(cmds, state.assets->models, state.assets->mapMaterials->get(mat_box)->id, viewProj, model, scale);
    // }

    EndRenderCommands(cmds);
}
#endif

internal void
RenderFrame(game_state& state, render_context& rc)
{
    GfxRenderTargetView screenRTV = gfx.AcquireNextSwapChainTarget(gfx.device);

    GfxCmdContext& cmds = state.cmds;

    gfx.ResetCmdEncoderPool(state.cmdpool);
    gfx.BeginEncodingCmds(cmds);

    GfxRenderTargetBarrier barrierRender = { screenRTV, GfxResourceState::Present, GfxResourceState::RenderTarget };
    gfx.CmdResourceBarrier(cmds, 0, nullptr, 0, nullptr, 1, &barrierRender);

    gfx.CmdSetViewport(cmds, 0, 0, rc.renderDimensions.Width, rc.renderDimensions.Height);
    gfx.CmdSetScissorRect(cmds, 0, 0, (u32)rc.renderDimensions.Width, (u32)rc.renderDimensions.Height);

    gfx.CmdBindRenderTargets(cmds, 1, &screenRTV, nullptr);
    gfx.CmdBindKernel(cmds, state.mainKernel);

    GfxDescriptor descriptors[] = {
        { GfxDescriptorType::Buffer, 0, "", state.materialBuffer }
    };

    GfxDescriptorSet desc = {};
    desc.setLocation = 0;   
    desc.count = ARRAY_COUNT(descriptors);
    desc.pDescriptors = descriptors;
    gfx.CmdBindDescriptorSet(cmds, desc);

    // TODO(james): send the position of the geometry

    render_geometry& gm = state.box.geometry;
    gfx.CmdBindIndexBuffer(cmds, gm.indexBuffer);
    gfx.CmdBindVertexBuffer(cmds, gm.vertexBuffer);
    gfx.CmdDrawIndexed(cmds, gm.indexCount, 1, 0, 0, 0);

    gfx.CmdBindRenderTargets(cmds, 0, nullptr, nullptr);    // Unbind the render targets so we can move the RTV to the Present Mode

    GfxRenderTargetBarrier barrierPresent = { screenRTV, GfxResourceState::RenderTarget, GfxResourceState::Present };
    gfx.CmdResourceBarrier(cmds, 0, nullptr, 0, nullptr, 1, &barrierPresent);

    gfx.EndEncodingCmds(cmds);

    gfx.SubmitCommands(gfx.device, 1, &cmds);

    b32 vsync = true;
    gfx.Frame(gfx.device, vsync);
}

internal void
SetupRenderResources(game_state& state, render_context& render)
{

}



platform_api Platform;
extern "C"
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Platform = gameMemory.platformApi;
    gfx = render.gfx;
    
    // TODO(james): Use bootstrap arena to allocate and initialize game state!!!

    if(!gameMemory.state)
    {
        gameMemory.state = BootstrapPushStructMember(game_state, totalArena);
        game_state& gameState = *gameMemory.state;
        gameState.frameArena = BootstrapScratchArena("FrameArena", NonRestoredArena(Megabytes(1)));
        gameState.temporaryFrameMemory = BeginTemporaryMemory(*gameState.frameArena);

        // gameState.resourceQueue = render.resourceQueue;   
        gameState.assets = AllocateGameAssets(gameState);
     
        gameState.camera.position = Vec3(1.0f, 5.0f, 5.0f);
        gameState.camera.target = Vec3(0.0f, 0.0f, 0.0f);
        gameState.cameraProjection = Perspective(45.0f, render.renderDimensions.Width, render.renderDimensions.Height, 0.1f, 100.0f);

        // NOTE(james): Values taken from testing to setup a good starting point
        //gameState.position = Vec3(0.218433440f,0.126181871f,0.596520841f);
        // gameState.scaleFactor = 0.0172703639f;
        // gameState.rotationAngle = 120.188347f;
        gameState.lightPosition = Vec3(1.2f, 3.0f, 2.0f);
        gameState.lightScale = 0.2f;


        gameState.position = Vec3i(0,0,0);
        gameState.scaleFactor = 1.0f;
        //gameState.rotationAngle = 120.188347f;

        {
            f32 width = 1.0f;//render.renderDimensions.Width;
            f32 height = 1.0f;//render.renderDimensions.Height;
            f32 halfWidth = width/2.0f;
            f32 halfHeight = height/2.0f;

            umm posOffset = OffsetOf(render_mesh_vertex, pos);
            umm colorOffset = OffsetOf(render_mesh_vertex, color);
            umm uvOffset = OffsetOf(render_mesh_vertex, texCoord);
            umm vsize = sizeof(render_mesh_vertex);

            render_mesh_vertex vertices[] = {
                {{ -halfWidth,  halfHeight, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }},
                {{ -halfWidth, -halfHeight, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }},
                {{  halfWidth, -halfHeight, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }},
                {{  halfWidth,  halfHeight, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }},
            };
            u32 indices[] = { 0, 1, 2, 0, 2, 3 };

            umm totalSize = sizeof(vertices);
            umm elemSize = sizeof(vertices[0]);

            GfxColor color = gfxColor(0.0f, 0.0f, 1.0f);

            GfxBufferDesc vb = MeshVertexBuffer(ARRAY_COUNT(vertices));
            GfxBufferDesc ib = IndexBuffer(ARRAY_COUNT(indices));
            GfxBufferDesc mb = UniformBuffer(sizeof(GfxColor));

            gameState.materialBuffer = gfx.CreateBuffer(gfx.device, mb, &color);
            gameState.box.geometry.indexCount = ARRAY_COUNT(indices);
            gameState.box.geometry.indexBuffer = gfx.CreateBuffer(gfx.device, ib, indices);
            gameState.box.geometry.vertexBuffer = gfx.CreateBuffer(gfx.device, vb, vertices);

            // CopyArray(ARRAY_COUNT(indices), indices, gfx.GetBufferData(gfx.device, gameState.box.geometry.indexBuffer));
            // CopyArray(ARRAY_COUNT(vertices), vertices, gfx.GetBufferData(gfx.device, gameState.box.geometry.vertexBuffer));

            gameState.shaderProgram = LoadProgram(*gameState.frameArena, "shader.vert.spv", "shader.frag.spv");
            gameState.mainKernel = gfx.CreateGraphicsKernel(gfx.device, gameState.shaderProgram, DefaultPipeline());
            gameState.cmdpool = gfx.CreateEncoderPool(gfx.device, {GfxQueueType::Graphics});
            gameState.cmds = gfx.CreateEncoderContext(gameState.cmdpool);
        }
    }    
    
    game_state& gameState = *gameMemory.state;

    // NOTE(james): Setup scratch memory for the frame...
    EndTemporaryMemory(gameState.temporaryFrameMemory);
    gameState.temporaryFrameMemory = BeginTemporaryMemory(*gameState.frameArena);

    // simple rotation update
    GameClock& clock = input.clock;
    //gameState.skullRotationAngle += clock.elapsedFrameTime * 90.0f;
    const f32 velocity = 1.5f * clock.elapsedFrameTime;
    const f32 scaleRate = 0.4f * clock.elapsedFrameTime;
    const f32 rotRate = 360.0f * clock.elapsedFrameTime;
    
    for(int controllerIndex = 0; controllerIndex < 5; ++controllerIndex)
    {
        InputController& controller = input.controllers[controllerIndex];
        if(controller.isConnected)
        {
            if(controller.isAnalog)
            {
                v2 lstick = Vec2(controller.leftStick.x, controller.leftStick.y);
                f32 magnitude = LengthVec2(lstick);                

                if(magnitude > 0)
                {
                    v2 norm_lstick = NormalizeVec2(lstick);
                    
                    v3 offset = Vec3(norm_lstick.X, 0.0f, -norm_lstick.Y) * (magnitude * velocity);
                    gameState.position += offset;
                }

                v2 rstick = Vec2(controller.rightStick.x, controller.rightStick.y);
                magnitude = LengthVec2(rstick);

                if(magnitude > 0)
                {
                    v2 norm_rstick = NormalizeVec2(rstick);

                    v3 offset = Vec3(norm_rstick.X, 0.0f, -norm_rstick.Y) * (magnitude * velocity);
                    gameState.camera.position += offset;
                }

                gameState.scaleFactor *= (1.0f - (scaleRate * controller.leftTrigger.value));
                gameState.scaleFactor *= (1.0f + (scaleRate * controller.rightTrigger.value));
            }

            if(controller.up.pressed)
            {
                gameState.camera.position.Y += velocity;
            }

            if(controller.down.pressed)
            {
                gameState.camera.position.Y -= velocity;
            }

            if(controller.rightStickButton.pressed)
            {
                gameState.camera.position = Vec3(1.0f, 5.0f, 5.0f);
            }

            if(controller.leftShoulder.pressed)
            {
                gameState.rotationAngle += rotRate;
            }
            if(controller.rightShoulder.pressed)
            {
                gameState.rotationAngle -= rotRate;
            }

            if(controller.y.pressed)
            {
                gameState.position.Y += velocity;
            }
            if(controller.a.pressed)
            {
                gameState.position.Y -= velocity;
            }

            if(gameState.rotationAngle > 360.0f)
            {
                gameState.rotationAngle -= 360.0f;
            }
            else if(gameState.rotationAngle < 0.0f)
            {
                gameState.rotationAngle += 360.0f;
            }
        }
    }

    RenderFrame(gameState, render);
    
    // BuildRenderCommands(gameState, render, input.clock);
    
#if 0
    // TODO(james): needs a better allocation scheme 
    GameTestState& gameState = *(GameTestState*)frame.persistantMemory.basePointer;
    if(frame.persistantMemory.basePointer == frame.persistantMemory.freePointer)
    {
        PushStruct(frame.persistantMemory, sizeof(gameState));
        // memory isn't initialized
        gameState.x_offset = 0;
        gameState.y_offset = 0;
        
        // 6 (E)	329.63 Hz	E4
        // 5 (B)	246.94 Hz	B4
        // 4 (G)	196.00 Hz	G3
        // 3 (D)	146.83 Hz	D3
        // 2 (A)	110.00 Hz	A3
        // 1 (E)	82.41 Hz	E2
        
        f32 frequencies[] = {
            CalcFrequency(2,7),
            CalcFrequency(3,0),
            CalcFrequency(3,5),
            CalcFrequency(3,10),
            CalcFrequency(4,2),
            CalcFrequency(4,7)
        };
        
        const AudioContextDesc& desc = audio.descriptor;
        
        for(int toneIndex = 0; toneIndex < 6; ++toneIndex)
        {
            SoundTone& tone = gameState.tones[toneIndex];
            tone.toneIndex = toneIndex;
            tone.isActive = false;
            tone.playbackSampleIndex = 0;
            
            f32 freq = frequencies[toneIndex];
            // now fill out the samples
            u32 samplesPerPeriod = (u32)(((f32)desc.samplesPerSecond/freq) + 0.5f);
            tone.sample_count = samplesPerPeriod;
            tone.tone_samples = (nf32*)PushArray(frame.persistantMemory, sizeof(nf32), tone.sample_count);
            
            f32 phase = 0.0f;
            nf32 lastValue = 0.0f;
            nf32* toneSamples = tone.tone_samples;
            for(u32 sampleIndex = 0; sampleIndex < tone.sample_count; ++sampleIndex)
            {
                lastValue = Oscilator(phase, freq, (f32)desc.samplesPerSecond, lastValue);
                toneSamples[sampleIndex] = lastValue;
            }
        }
        
        ZeroArray(ARRAY_COUNT(gameState.lastSecondAudioSamples), gameState.lastSecondAudioSamples);
        gameState.indexIntoLastSecondSamples = 0;
    }
    
    for(int controllerIndex = 0; controllerIndex < 5; ++controllerIndex)
    {
        InputController& controller = input.controllers[controllerIndex];
        
        if(controller.isConnected)
        {
            if(controller.isAnalog)
            {
                gameState.x_offset += (int)(controller.leftStick.x * 5.0f);
                gameState.y_offset += (int)(controller.leftStick.y * 5.0f);
                
                // frequency range of 80Hz -> 1200Hz
                // const real32 min = 250.0f;
                // const real32 max = 278.0f;
                // const real32 halfMagnitude = (max - min)/2.0f;
                // gameState.toneHz = min + halfMagnitude + controller.rightStick.y * halfMagnitude;
            }
            
            if(controller.left.pressed)
            {
                gameState.x_offset -= 10;
            }
            if(controller.right.pressed)
            {
                gameState.x_offset += 10;
            }
            if(controller.up.pressed)
            {
                gameState.y_offset += 10;
            }
            if(controller.down.pressed)
            {
                gameState.y_offset -= 10;
            }
            
            
            for(int toneIndex = 0; toneIndex < ARRAY_COUNT(gameState.tones); ++toneIndex)
            {
                ToggleSoundTone(gameState.tones[toneIndex], controller.buttons[toneIndex]);
            }
            
            controller.leftFeedbackMotor = controller.leftTrigger.value;
            controller.rightFeedbackMotor = controller.rightTrigger.value;
        }
    }
    
    FillSoundBuffer(audio, gameState, frame);
    RenderScene(graphics, gameState, frame);
    
    now keep a ring buffer copy of the frames audio samples
        u32 sampleCountToCopy = audio.samplesWritten * 2;
    u32 nextIndex = gameState.indexIntoLastSecondSamples + sampleCountToCopy; 
    i16* audioBuffer = (i16*)audio.streamBuffer.data;
    const AudioContextDesc& desc = audio.descriptor;
    
    const u32 maxIndex = (desc.samplesPerSecond * desc.numChannels) - 1;
    
    if(nextIndex > maxIndex)
    {
        u32 entriesToEnd = (maxIndex - gameState.indexIntoLastSecondSamples);
        // have to do 2 copies
        CopyArray(entriesToEnd, audioBuffer, &gameState.lastSecondAudioSamples[gameState.indexIntoLastSecondSamples]);
        u32 secondIndex = sampleCountToCopy - entriesToEnd;
        CopyArray(secondIndex, &audioBuffer[entriesToEnd], gameState.lastSecondAudioSamples);
        nextIndex = secondIndex;
    }
    else
    {
        CopyArray(sampleCountToCopy, audioBuffer, &gameState.lastSecondAudioSamples[gameState.indexIntoLastSecondSamples]);
    }
    gameState.indexIntoLastSecondSamples = nextIndex;
    
    //RenderGradient(graphics, gameState); 
    RenderAudioWave(graphics, gameState);
#endif
}