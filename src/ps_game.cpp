#include "ps_game.h"
#include "ps_audio_synth.cpp"
#include "ps_render.cpp"
#include "ps_asset.cpp"

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

internal void
FillSoundBuffer(AudioContext& audio, game_state& game)
{
    const AudioContextDesc& desc = audio.descriptor;
    u32 numRequestedSamples = audio.samplesRequested;
    // we'll accumulate the samples in floating point
    f32* toneSamples = (f32*)PushArray(*game.frameArena, numRequestedSamples, f32);
    // clear the samples to 0
    ZeroArray(numRequestedSamples, toneSamples);
    
    // TODO(james): Implement playing sounds / music
#if 0
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
                if(tone.playbackSampleIndex >= tone.sample_count)
                {
                     tone.playbackSampleIndex = 0;
                }
            }
        }        
    }
#endif   
    
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

platform_api Platform;
extern "C"
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Platform = gameMemory.platformApi;
    gfx = graphics.gfx;
    
    if(!gameMemory.state)
    {
        gameMemory.state = BootstrapPushStructMember(game_state, totalArena);
        game_state& gameState = *gameMemory.state;
        gameState.frameArena = BootstrapScratchArena("FrameArena", NonRestoredArena(Megabytes(1)));
        gameState.temporaryFrameMemory = BeginTemporaryMemory(*gameState.frameArena);

        gameState.renderer = BootstrapPushStructMember(render_context, arena);
        gameState.renderer->frameArena = gameState.frameArena;
        gameState.renderer->gc = &graphics;

        SetupRenderer(gameState);
        
        // gameState.resourceQueue = render.resourceQueue;   
        gameState.assets = AllocateGameAssets(gameState);
     
        gameState.camera.position = Vec3(0.0f, 0.0f, 50.0f);
        gameState.camera.target = Vec3(0.0f, 0.0f, 0.0f);
        gameState.cameraProjection = Perspective(45.0f, graphics.windowWidth, graphics.windowHeight, 0.1f, 100.0f);

        // NOTE(james): Values taken from testing to setup a good starting point
        //gameState.position = Vec3(0.218433440f,0.126181871f,0.596520841f);
        // gameState.scaleFactor = 0.0172703639f;
        // gameState.rotationAngle = 120.188347f;
        gameState.lightPosition = Vec3(-5.0f, 0.0f, 10.0f);
        gameState.lightScale = 0.2f;

        gameState.position = Vec3i(0,5,0);
        gameState.scaleFactor = 1.0f;
        //gameState.rotationAngle = 120.188347f;
    }    
    
    game_state& gameState = *gameMemory.state;

    // NOTE(james): Setup scratch memory for the frame...
    EndTemporaryMemory(gameState.temporaryFrameMemory);
    gameState.temporaryFrameMemory = BeginTemporaryMemory(*gameState.frameArena);

    // simple rotation update
    GameClock& clock = input.clock;
    //gameState.skullRotationAngle += clock.elapsedFrameTime * 90.0f;
    const f32 velocity = 1.5f * clock.elapsedFrameTime;
    const f32 digitalVelocity = 20.0f * clock.elapsedFrameTime;
    const f32 scaleRate = 0.4f * clock.elapsedFrameTime;
    const f32 rotRate = 90.0f * clock.elapsedFrameTime;

    //gameState.rotationAngle += rotRate;
    
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
                    gameState.lightPosition += Vec3(norm_lstick.X, norm_lstick.Y, 0.0f) * (magnitude * velocity);
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

            if(controller.left.pressed)
            {
                gameState.camera.position.X += digitalVelocity;
            }

            if(controller.right.pressed)
            {
                gameState.camera.position.X -= digitalVelocity;
            }

            if(controller.up.pressed)
            {
                gameState.camera.position.Z -= digitalVelocity;
            }

            if(controller.down.pressed)
            {
                gameState.camera.position.Z += digitalVelocity;
            }

            if(controller.rightShoulder.pressed)
            {
                gameState.camera.position.Y += digitalVelocity;
            }

            if(controller.leftShoulder.pressed)
            {
                gameState.camera.position.Y -= digitalVelocity;
            }

            if(controller.rightStickButton.pressed)
            {
                gameState.camera.position = Vec3(1.0f, 5.0f, 5.0f);
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

    gameState.camera.target = gameState.position;
    
    FillSoundBuffer(audio, gameState);    
    RenderFrame(*gameState.renderer, gameState, input.clock);
}