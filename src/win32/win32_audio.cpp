
#include <Audioclient.h>
#include <mmdeviceapi.h>

DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);    // DEVPROP_TYPE_STRING

#define REF_MILLI(milli) ((REFERENCE_TIME)((milli) * 10000))
#define REF_SECONDS(seconds) (REF_MILLI(seconds) * 1000)

#define MILLI_REF(ref) ((ref) / 10000.0)
#define SECONDS_REF(ref) (MILLI_REF(ref) / 1000.0)

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);


internal void
Win32InitSoundDevice(Win32AudioContext& audio)
{
    AudioContextDesc& desc = audio.gameAudioBuffer.descriptor;

    // TODO(james): Enumerate available sound devices and/or output formats
    desc.samplesPerSecond = 48000;
    desc.numChannels = 2;
    desc.bitsPerSample = 16;
    const uint32 samplesPerSecond = desc.samplesPerSecond;
    const uint16 nNumChannels = desc.numChannels;
    const uint16 nNumBitsPerSample = desc.bitsPerSample;

    // now acquire access to the audio hardware

    // buffer size is expressed in terms of duration (presumably calced against the sample rate etc..)
    REFERENCE_TIME frameTime = REF_SECONDS(1.0/120.0);
    REFERENCE_TIME bufferTime = frameTime * 2 * nNumChannels;    // 2 frames worth

    HRESULT hr = 0;

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, 0, CLSCTX_ALL, 
        IID_IMMDeviceEnumerator, (void**)&audio.pEnumerator
        );
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }

    hr = audio.pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &audio.pDevice);
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }

    PROPVARIANT varName;
    IPropertyStore* pProps = 0;
    hr = audio.pDevice->OpenPropertyStore(STGM_READ, &pProps);
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }
    PropVariantInit(&varName);

    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName); 
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }

    LOG_INFO("Using Audio Device: %S", varName.pwszVal);

    PropVariantClear(&varName);
    COM_RELEASE(pProps);

    // TODO(james): Ask for ISpatialAudioClient instead for automatic 7.1.4 effects?
    hr = audio.pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, 0, (void**)&audio.pClient);
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }

    // Negotiate an exclusive mode audio stream with the device
    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = nNumChannels;
    wave_format.nSamplesPerSec = samplesPerSecond;
    wave_format.nBlockAlign = nNumChannels * nNumBitsPerSample / 8;
    wave_format.nAvgBytesPerSec = samplesPerSecond * wave_format.nBlockAlign;    
    wave_format.wBitsPerSample = nNumBitsPerSample;

    REFERENCE_TIME defLatency = 0;
    REFERENCE_TIME minLatency = 0;
    hr = audio.pClient->GetDevicePeriod(&defLatency, &minLatency);
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }

    bufferTime += minLatency;

    LOG_INFO("Audio Caps\n\tDefault Period: %.02f ms\n\tMinimum Period: %.02f ms\n\tRequested Period: %.02f ms\n", defLatency/10000.0f, minLatency/10000.0f, bufferTime/10000.0f);
    
    hr = audio.pClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &wave_format, 0);  // can't have a closest format in exclusive mode
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }
    

    hr = audio.pClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE, 0,
            bufferTime, bufferTime, 
            &wave_format, 0
        );
    if(hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
        // align the buffer I guess
        UINT32 nFrames = 0;
        hr = audio.pClient->GetBufferSize(&nFrames);
        if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }

        bufferTime = (REFERENCE_TIME)((double)REF_SECONDS(1) / samplesPerSecond * nFrames + 0.5);
        hr = audio.pClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE, 0,
            bufferTime, bufferTime, 
            &wave_format, 0
        );
    }
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }
    
    REFERENCE_TIME maxLatency = 0;
    hr = audio.pClient->GetStreamLatency(&maxLatency);
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }
    LOG_INFO("\tMax Latency: %.02f ms\n", maxLatency/10000.0f);
    // Create an event handle to signal Windows that we've got a
    // new audio buffer available
    // HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);  // probably should be global
    // if(!hEvent) { /* TODO(james): logging */ return; }

    // hr = g_pAudioClient->SetEventHandle(hEvent);
    // if(FAILED(hr)) { /* TODO(james): log error */ return; }

    // Get the actual size of the two allocated buffers
    UINT32 nBufferFrameCount = 0;
    hr = audio.pClient->GetBufferSize(&nBufferFrameCount);
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }
    
    // target the minimum latency value of the audio hardware
    audio.targetBufferFill = (uint32)(((1.5/120.0) + SECONDS_REF(minLatency*2)) * samplesPerSecond + 0.5);

    hr = audio.pClient->GetService(IID_IAudioRenderClient, (void**)&audio.pRenderClient);
    if(FAILED(hr)) { LOG_ERROR("Error: %x", hr); return; }
    
        // setup the game audio buffer
    buffer& audioBuffer = audio.gameAudioBuffer.streamBuffer;
    audioBuffer.size = nBufferFrameCount * nNumChannels * nNumBitsPerSample / 8; // buffer size here is only as big as the buffer size of the main audio buffer 
    audioBuffer.data = (u8*)VirtualAlloc(0, audioBuffer.size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    audio.gameAudioBuffer.samplesRequested = audio.targetBufferFill; // request a full buffer initially

    ZeroBuffer(audioBuffer);
}

internal void
Win32CopyAudioBuffer(Win32AudioContext& audio, float fFrameTimeStep)
{
    HRESULT hr = 0;
    const AudioContextDesc& desc = audio.gameAudioBuffer.descriptor;

    UINT32 curPadding = 0;
    hr = audio.pClient->GetCurrentPadding(&curPadding);

    if(FAILED(hr))
    {
        LOG_ERROR("GetCurrentPadding Error: 0x%x", hr);
        ASSERT(false);
    }

    BYTE* pBuffer = 0;
    uint32 numSamples = audio.gameAudioBuffer.samplesWritten;
    int32 maxAvailableBuffer = audio.targetBufferFill - curPadding;

#if 0
    real32 curAudioLatencyMS = (real32)curPadding/(real32)desc.samplesPerSecond * 1000.0f;
    LOG_DEBUG("Cur Latency: %.02fms\t\tAvail. Buffer: %d\tPadding: %d\tWrite Frame Samples: %d\n", curAudioLatencyMS, maxAvailableBuffer, curPadding, numSamples);
#endif

    //ASSERT(numSamples <= maxAvailableBuffer);
    // update the number of samples requested from the next frame
    // based on the amount of available buffer space and the current
    // frame rate
    audio.gameAudioBuffer.samplesRequested = 
        (uint32)(fFrameTimeStep * desc.samplesPerSecond)
        + (maxAvailableBuffer - numSamples);

    hr = audio.pRenderClient->GetBuffer(numSamples, &pBuffer);
    if(SUCCEEDED(hr))
    {
        // copy data to buffer
        // int16 *SampleOut = (int16*)pBuffer;
        // int16 *SampleIn = (int16*)audio.gameAudioBuffer.buffer;

        CopyArray(audio.gameAudioBuffer.samplesWritten * desc.numChannels, (int16*)audio.gameAudioBuffer.streamBuffer.data, pBuffer);
        // for(uint32 sampleIndex = 0;
        //     sampleIndex < numSamples;
        //     ++sampleIndex)
        // {
        //     *SampleOut++ = *SampleIn++;
        //     *SampleOut++ = *SampleIn++;
        // }

        hr = audio.pRenderClient->ReleaseBuffer(numSamples, 0);
        if(FAILED(hr)) { LOG_ERROR("ReleaseBuffer Error: 0x%x", hr); }
    }
    else
    {
        ASSERT(false);
        LOG_ERROR("GetBuffer Error: 0x%x", hr);
    }

}