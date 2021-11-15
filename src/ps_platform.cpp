
#include "ps_platform.h"

struct SoundTone
{
    b32 isActive;
    f32 freq;
    f32 tPhase;
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
RenderAudioWave(const GraphicsContext& graphics, GameTestState& gameState)
{
    // first clear the buffer
    //memset(graphics.buffer, 0, graphics.buffer_height*graphics.buffer_pitch);

    u32* pixels = (u32*)graphics.buffer;
    u32* row = pixels;
    for(u32 y = 0; y < graphics.buffer_height; ++y)
    {
        u32* col = row;
        for(u32 x = 0; x < graphics.buffer_width; ++x)
        {
            *col++ = 0x00000000;
        }
        row += graphics.buffer_width;
    }

    // now render the audio wave
    //u32* pixels = (u32*)graphics.buffer;
    i16* audioSamples = gameState.lastSecondAudioSamples; 
    int samplesPerColumn = 48000 / graphics.buffer_width;

    int midY = (int)graphics.buffer_height / 2;

    u32 leftColor = 255;        // blue
    u32 rightColor = 255 << 16; // red

    for(u32 x = 0; x < graphics.buffer_width; ++x)
    {
        // now average the normalized sample values (-1..1)
        f32 fLeftSampleValue = 0.0f;
        f32 fRightSampleValue = 0.0f;
        for(int sampleIndex = 0; sampleIndex < samplesPerColumn; ++sampleIndex)
        {
            fLeftSampleValue = (f32)*audioSamples++ / 0x7FFF;
            fRightSampleValue = (f32)*audioSamples++ / 0x7FFF;

            // now map the -1..1 sample values into the y coordinate
            int ly = midY + (int)(fLeftSampleValue * (midY));
            int ry = midY + (int)(fRightSampleValue * (midY));

            pixels[(ly * graphics.buffer_width) + x] = leftColor;
            pixels[(ry * graphics.buffer_width) + x] = rightColor;
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

internal inline f32
Oscilator_Sine(f32& phase, f32 frequency, f32 sampleRate)
{
    const f32 twoPi = 2.0f*Pi32;

    phase += twoPi*frequency/sampleRate;

    if(phase > twoPi)
    {
        phase -= twoPi;
    }

    return sinf(phase);
}

internal inline f32
Oscilator_Square(f32& phase, f32 frequency, f32 sampleRate)
{
    phase += frequency/sampleRate;

	while(phase > 1.0f)
		phase -= 1.0f;

	while(phase < 0.0f)
		phase += 1.0f;

	if(phase <= 0.5f)
		return -1.0f;
	else
		return 1.0f;
}

internal inline
float Oscilator_Saw(float &fPhase, float fFrequency, float fSampleRate)
{
	fPhase += fFrequency/fSampleRate;

	while(fPhase > 1.0f)
		fPhase -= 1.0f;

	while(fPhase < 0.0f)
		fPhase += 1.0f;

	return (fPhase * 2.0f) - 1.0f;
}

internal inline
float Oscilator_Triangle(float &fPhase, float fFrequency, float fSampleRate)
{
	fPhase += fFrequency/fSampleRate;

	while(fPhase > 1.0f)
		fPhase -= 1.0f;

	while(fPhase < 0.0f)
		fPhase += 1.0f;

	float fRet;
	if(fPhase <= 0.5f)
		fRet=fPhase*2;
	else
		fRet=(1.0f - fPhase)*2;

	return (fRet * 2.0f) - 1.0f;
}

float AdvanceOscilator_Noise(float &fPhase, float fFrequency, float fSampleRate, float fLastValue)
{
	unsigned int nLastSeed = (unsigned int)fPhase;
	fPhase += fFrequency/fSampleRate;
	unsigned int nSeed = (unsigned int)fPhase;

	while(fPhase > 2.0f)
		fPhase -= 1.0f;

	if(nSeed != nLastSeed)
	{
		float fValue = ((float)rand()) / ((float)RAND_MAX);
		fValue = (fValue * 2.0f) - 1.0f;

		//uncomment the below to make it slightly more intense
		/*
		if(fValue < 0)
			fValue = -1.0f;
		else
			fValue = 1.0f;
		*/

		return fValue;
	}
	else
	{
		return fLastValue;
	}
}

internal void
FillSoundBuffer(AudioContext& audio, GameTestState& gameState, GameContext& game)
{
    u32 numRenderSamples = audio.samplesRequested;
    void* transientMemoryPosition = game.transientMemory.freePointer;
    // we'll accumulate the samples in floating point
    f32* toneSamples = (f32*)PushArray(game.transientMemory, sizeof(f32), numRenderSamples);
    // clear the samples to 0
    ZeroArray(numRenderSamples, toneSamples);
    
    for(int soundToneIndex = 0; soundToneIndex < 6; ++soundToneIndex)
    {
        SoundTone& tone = gameState.tones[soundToneIndex];

        // TODO(james): Handle stereo
        f32 samplesPerPeriod = tone.freq/(f32)audio.samplesPerSecond;

        // f32 lastValue = 0.0f;
        // now accumalate the samples for this tone
        for(u32 sampleIndex = 0; sampleIndex < numRenderSamples; ++sampleIndex)
        {
            if(tone.isActive)
            {
                f32 value = Oscilator_Sine(tone.tPhase, tone.freq, (f32)audio.samplesPerSecond);
                toneSamples[sampleIndex] += value;
                //lastValue = value;
            }
            else
            {
                tone.tPhase = 0.0f;
            }
        }
    
    }

    // Now we'll take the accumulated samples and convert to 16-bit and add in the volume
    
    uint16 toneVolume = 1000;

    // convert the timestep to the number of samples we need to render
    //   samples = 2 channels of 16 bit audio
    
    //   LEFT RIGHT | LEFT RIGHT | LEFT RIGHT | ...
    
    audio.samplesWritten = numRenderSamples;    
        
    // copy data to buffer
    int16 *SampleOut = (int16*)audio.buffer;
    for(uint32 sampleIndex = 0;
        sampleIndex < numRenderSamples;
        ++sampleIndex)
    {
        ASSERT(toneSamples[sampleIndex]*toneSamples[sampleIndex] < 1.0);
        int16 SampleValue = (int16)(toneSamples[sampleIndex] * toneVolume);
        // TODO(james): Handle stereo
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
    }

    // reset our scratch memory
    game.transientMemory.freePointer = transientMemoryPosition;
}

//calculate the frequency of the specified note.
//fractional notes allowed!
f32 CalcFrequency(f32 fOctave,f32 fNote)
/*
	Calculate the frequency of any note!
	frequency = 440×(2^(n/12))
	N=0 is A4
	N=1 is A#4
	etc...
	notes go like so...
	0  = A
	1  = A#
	2  = B
	3  = C
	4  = C#
	5  = D
	6  = D#
	7  = E
	8  = F
	9  = F#
	10 = G
	11 = G#
*/
{
    double freq = 440*pow(2.0, fNote / 12.0); 
	return (float)(440*pow(2.0,((double)((fOctave-4)*12+fNote))/12.0));
}

extern "C"
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    // TODO(james): needs a better allocation scheme 
    GameTestState& gameState = *(GameTestState*)gameContext.persistantMemory.basePointer;
    if(gameContext.persistantMemory.basePointer == gameContext.persistantMemory.freePointer)
    {
        gameContext.persistantMemory.freePointer = ((uint8*)gameContext.persistantMemory.freePointer) + sizeof(gameState);
        // memory isn't initialized
        gameState.x_offset = 0;
        gameState.y_offset = 0;

        // 6 (E)	329.63 Hz	E4
        // 5 (B)	246.94 Hz	B3
        // 4 (G)	196.00 Hz	G3
        // 3 (D)	146.83 Hz	D3
        // 2 (A)	110.00 Hz	A2
        // 1 (E)	82.41 Hz	E2

        f32 frequencies[] = {
            CalcFrequency(2,7),
            CalcFrequency(2,0),
            CalcFrequency(3,5),
            CalcFrequency(3,10),
            CalcFrequency(3,2),
            CalcFrequency(4,7)
        };
        
        for(int toneIndex = 0; toneIndex < 6; ++toneIndex)
        {
            SoundTone& tone = gameState.tones[toneIndex];
            tone.isActive = false;
            tone.freq = frequencies[toneIndex];
            tone.tPhase = 0.0f;
        }

        memset(&gameState.lastSecondAudioSamples, 0, sizeof(gameState.lastSecondAudioSamples));
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

            if(controller.isAnalog)
            {
                // this won't work with multiple active controllers...
                gameState.tones[0].isActive = controller.a.pressed;
                gameState.tones[1].isActive = controller.b.pressed;
                gameState.tones[2].isActive = controller.y.pressed;
                gameState.tones[3].isActive = controller.x.pressed;
                gameState.tones[4].isActive = controller.leftShoulder.pressed;
                gameState.tones[5].isActive = controller.rightShoulder.pressed;
            }
            controller.leftFeedbackMotor = controller.leftTrigger.value;
            controller.rightFeedbackMotor = controller.rightTrigger.value;
        }
    }

    FillSoundBuffer(audio, gameState, gameContext);

    // now keep a ring buffer copy of the frames audio samples
    u32 nextIndex = gameState.indexIntoLastSecondSamples + (audio.samplesWritten * 2); 

    if(nextIndex > 48000*2)
    {
        // have to do 2 copies
        memcpy(&gameState.lastSecondAudioSamples[gameState.indexIntoLastSecondSamples], audio.buffer, ((48000*2) - gameState.indexIntoLastSecondSamples) * sizeof(i16));
        nextIndex -= 48000*2;
        memcpy(&gameState.lastSecondAudioSamples[0], ((i16*)audio.buffer + ((48000*2) - gameState.indexIntoLastSecondSamples)), nextIndex);
    }
    else
    {
        memcpy(&gameState.lastSecondAudioSamples[gameState.indexIntoLastSecondSamples], audio.buffer, audio.samplesWritten * 2 * sizeof(i16));
    }
    gameState.indexIntoLastSecondSamples = nextIndex;

    //RenderGradient(graphics, gameState); 
    RenderAudioWave(graphics, gameState);
}