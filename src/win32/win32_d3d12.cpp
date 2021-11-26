
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>    // internal only? Compilation should be done offline - Requires d3dcompiler.lib and a DLL

#include "win32_d3d12.h"

#ifdef PROJECTSUPER_INTERNAL
#define DEBUG_GRAPHICS 1
#endif

/*******************************************************************************

    Things that still need to be handled:

    * Backbuffer resize on Window Resize
    * Fullscreen borderless window
    * True fullscreen?  --This really isn't necessary anymore
    * Query for capabilities of hardware
        * What specifically am I aiming to support here?
    * Verify that we're using the up-to-date interfaces, tutorial from 2017
    * Begin to abstract a common command based API for the game
        * Setup resource transfer queue
    * Separate into an independent DLL
    * Move PSO and Shader gen/upload to background
    * Move resource upload to background

********************************************************************************/

// number of buffer frames 1 - Active, 2 Flip-Flop
const u8 g_num_frames = 3;

// Direct3D 12 
// TODO(james): Check MSDN docs to verify that these are the correct interface versions, ID3D12Device9?
global ID3D12Device2* g_pDevice;
global ID3D12DebugDevice* g_pDebugDevice;
global ID3D12CommandQueue* g_pCommandQueue;
global ID3D12GraphicsCommandList* g_pCommandList;
global ID3D12CommandAllocator* g_pCommandAllocators[g_num_frames];

// Current Frame
global IDXGISwapChain4* g_pSwapChain;
global ID3D12Resource* g_pBackBuffers[g_num_frames];
global ID3D12DescriptorHeap* g_RTVDescriptorHeap;
global UINT g_RTVDescriptorSize;
global UINT g_CurrentBackBufferIndex;

// sync objects
global ID3D12Fence* g_pFence;
global u64 g_FenceValue = 0;
global u64 g_FrameFenceValues[g_num_frames] = {};
global HANDLE g_FenceEvent;

global D3D12_VIEWPORT g_Viewport;
global D3D12_RECT g_SurfaceSize;

global ID3D12Resource* g_pVertexBuffer;
global ID3D12Resource* g_pIndexBuffer;

global ID3D12Resource* g_pUniformBuffer;
global ID3D12DescriptorHeap* g_pUniformBufferHeap;
global void* g_pMappedUniformBuffer;

global D3D12_VERTEX_BUFFER_VIEW g_VertexBufferView;
global D3D12_INDEX_BUFFER_VIEW g_IndexBufferView;

global ID3D12RootSignature* g_pRootSignature;
global ID3D12PipelineState* g_pPipelineState;

struct VS_Uniforms
{
    m4 projection;
    m4 view;
    m4 model;
};
global VS_Uniforms g_Uniforms;

struct Vertex
{
    float position[3];
    float color[3];
};

Vertex g_VertexBufferData[3] = {{{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                                {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
                                {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};

u32 g_IndexBufferData[3] = {0, 1, 2};


// config options
global bool g_use_warp = false;
global bool g_vsync = true;
global bool g_tearing_support = false;
global bool g_fullscreen = false;

#define CHECK_ERROR(hres) if(FAILED(hres)) { LOG_ERROR("D3D12 ERROR: %X", hres); ASSERT(false); }

internal void
Win32D3d12InitResources()
{
    HRESULT hr = ERROR_SUCCESS;
    // TODO(james): This is a temporary resource loading function, will be removed later
    
    // Root Signature
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If
        // CheckFeatureSupport succeeds, the HighestVersion returned will not be
        // greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(g_pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE,
                                                &featureData,
                                                sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        } 

        D3D12_DESCRIPTOR_RANGE1 ranges[1];
        ranges[0].BaseShaderRegister = 0;
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[0].NumDescriptors = 1;
        ranges[0].RegisterSpace = 0;
        ranges[0].OffsetInDescriptorsFromTableStart = 0;
        ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

        D3D12_ROOT_PARAMETER1 rootParameters[1];
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[0].DescriptorTable.pDescriptorRanges = ranges;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        rootSignatureDesc.Desc_1_1.NumParameters = 1;
        rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
        rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

        ID3DBlob* signature = nullptr;
        ID3DBlob* error = nullptr;

        hr = D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error);
        if(FAILED(hr))
        {
            LOG_ERROR("D3D12 ERROR: %s", (const char*)error->GetBufferPointer());
        }
        CHECK_ERROR(hr);

        hr = g_pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_pRootSignature));
        if(FAILED(hr))
        {
            LOG_ERROR("D3D12 ERROR: %s", (const char*)error->GetBufferPointer());
        }
        CHECK_ERROR(hr);

        COM_RELEASE(error);
        COM_RELEASE(signature);
    }

    // TODO(james): break compiling shaders out of this part into a seperate step.  Should be done offline...
    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ID3DBlob* vertexShader = nullptr;
        ID3DBlob* pixelShader = nullptr;
        ID3DBlob* errors = nullptr;

        UINT compileFlags = 0;

#if defined(DEBUG_GRAPHICS)
        // enable better shader debugging
        // TODO(james): should this be a different option that just a general DEBUG_GRAPHICS???
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        hr = D3DCompile(g_szVertexShader, StringLength(g_szVertexShader), "Simple Vertex Shader", nullptr, nullptr, 
            "main", "vs_5_0", compileFlags, 0, &vertexShader, &errors);       
        if(FAILED(hr))
        {
            LOG_ERROR("D3D12 ERROR: %s", (const char*)errors->GetBufferPointer());
        }
        CHECK_ERROR(hr);
        
        hr = D3DCompile(g_szPixelShader, StringLength(g_szPixelShader), "Simple Pixel Shader", nullptr, nullptr,
            "main", "ps_5_0", compileFlags, 0, &pixelShader, &errors);
        if(FAILED(hr))
        {
            LOG_ERROR("D3D12 ERROR: %s", (const char*)errors->GetBufferPointer());
        }
        CHECK_ERROR(hr);

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

        // Create the Uniform Buffer Object
        {
            // Note: using upload heaps to transfer static data like vert
            // buffers is not recommended. Every time the GPU needs it, the
            // upload heap will be marshalled over. Please read up on Default
            // Heap usage. An upload heap is used here for code simplicity and
            // because there are very few verts to actually transfer.
            D3D12_HEAP_PROPERTIES heapProps;
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
            heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProps.CreationNodeMask = 1;
            heapProps.VisibleNodeMask = 1;

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = 1;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            hr = g_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&g_pUniformBufferHeap));
            CHECK_ERROR(hr);

            D3D12_RESOURCE_DESC uboResourceDesc;
            uboResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            uboResourceDesc.Alignment = 0;
            uboResourceDesc.Width = (sizeof(g_Uniforms) + 255) & ~255;
            uboResourceDesc.Height = 1;
            uboResourceDesc.DepthOrArraySize = 1;
            uboResourceDesc.MipLevels = 1;
            uboResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            uboResourceDesc.SampleDesc.Count = 1;
            uboResourceDesc.SampleDesc.Quality = 0;
            uboResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            uboResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            hr = g_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &uboResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&g_pUniformBuffer));
            CHECK_ERROR(hr);
            g_pUniformBufferHeap->SetName(L"Constant Buffer Upload Resource Heap");

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = g_pUniformBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes =
                (sizeof(g_Uniforms) + 255) &
                ~255; // CB size is required to be 256-byte aligned.
            
            D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle(g_pUniformBufferHeap->GetCPUDescriptorHandleForHeapStart());
            cbvHandle.ptr += g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 0;

            g_pDevice->CreateConstantBufferView(&cbvDesc, cbvHandle);

            // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
            D3D12_RANGE readRange;
            readRange.Begin = 0;
            readRange.End = 0;

            hr = g_pUniformBuffer->Map(0, &readRange, &g_pMappedUniformBuffer);
            CHECK_ERROR(hr);
            Copy(sizeof(g_Uniforms), &g_Uniforms, g_pMappedUniformBuffer);
            g_pUniformBuffer->Unmap(0, &readRange);
        }

        // Describe and create the pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = {inputElementDescs, ARRAY_COUNT(inputElementDescs)};
        psoDesc.pRootSignature = g_pRootSignature;

        D3D12_SHADER_BYTECODE vsByteCode;
        D3D12_SHADER_BYTECODE psByteCode;

        vsByteCode.pShaderBytecode = vertexShader->GetBufferPointer();
        vsByteCode.BytecodeLength = vertexShader->GetBufferSize();
        psByteCode.pShaderBytecode = pixelShader->GetBufferPointer();
        psByteCode.BytecodeLength = pixelShader->GetBufferSize();

        psoDesc.VS = vsByteCode;
        psoDesc.PS = psByteCode;

        D3D12_RASTERIZER_DESC rasterDesc = {};
        rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterDesc.CullMode = D3D12_CULL_MODE_NONE;     // temporary
        rasterDesc.FrontCounterClockwise = FALSE;
        rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterDesc.DepthClipEnable = TRUE;
        rasterDesc.MultisampleEnable = FALSE;
        rasterDesc.AntialiasedLineEnable = FALSE;
        rasterDesc.ForcedSampleCount = 0;
        rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        psoDesc.RasterizerState = rasterDesc;

        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
            FALSE,
            FALSE,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        for(UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
        }

        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        hr = g_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pPipelineState));
        CHECK_ERROR(hr);

        COM_RELEASE(vertexShader);
        COM_RELEASE(pixelShader);
        COM_RELEASE(errors);
    }

    // Create the vertex buffer
    {
        const UINT vertexBufferSize = sizeof(g_VertexBufferData);

        // Note: using upload heaps to transfer static data like vert buffers is
        // not recommended. Every time the GPU needs it, the upload heap will be
        // marshalled over. Please read up on Default Heap usage. An upload heap
        // is used here for code simplicity and because there are very few verts
        // to actually transfer.
        D3D12_HEAP_PROPERTIES heapProps;
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC vertexBufferResourceDesc;
        vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        vertexBufferResourceDesc.Alignment = 0;
        vertexBufferResourceDesc.Width = vertexBufferSize;
        vertexBufferResourceDesc.Height = 1;
        vertexBufferResourceDesc.DepthOrArraySize = 1;
        vertexBufferResourceDesc.MipLevels = 1;
        vertexBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        vertexBufferResourceDesc.SampleDesc.Count = 1;
        vertexBufferResourceDesc.SampleDesc.Quality = 0;
        vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        vertexBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = g_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertexBufferResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_pVertexBuffer));
        
        // Copy the triangle data to the vertex buffer
        void* pMappedVertexData = 0;

        // Don't intend to read this data back
        D3D12_RANGE readRange;
        readRange.Begin = 0;
        readRange.End = 0;

        hr = g_pVertexBuffer->Map(0, &readRange, &pMappedVertexData);
        Copy(sizeof(g_VertexBufferData), &g_VertexBufferData, pMappedVertexData);
        g_pVertexBuffer->Unmap(0, nullptr);

        // initialize the vertex buffer view
        g_VertexBufferView.BufferLocation = g_pVertexBuffer->GetGPUVirtualAddress();
        g_VertexBufferView.StrideInBytes = sizeof(Vertex);
        g_VertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Now the index buffer
    {
        const UINT indexBufferSize = sizeof(g_IndexBufferData);

        // Note: using upload heaps to transfer static data like vert buffers is
        // not recommended. Every time the GPU needs it, the upload heap will be
        // marshalled over. Please read up on Default Heap usage. An upload heap
        // is used here for code simplicity and because there are very few verts
        // to actually transfer.
        D3D12_HEAP_PROPERTIES heapProps;
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC indexBufferResourceDesc;
        indexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        indexBufferResourceDesc.Alignment = 0;
        indexBufferResourceDesc.Width = indexBufferSize;
        indexBufferResourceDesc.Height = 1;
        indexBufferResourceDesc.DepthOrArraySize = 1;
        indexBufferResourceDesc.MipLevels = 1;
        indexBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        indexBufferResourceDesc.SampleDesc.Count = 1;
        indexBufferResourceDesc.SampleDesc.Quality = 0;
        indexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        indexBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = g_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &indexBufferResourceDesc, 
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_pIndexBuffer));

        void* pMappedIndexBuffer = 0;

        D3D12_RANGE readRange = {};

        hr = g_pIndexBuffer->Map(0, &readRange, &pMappedIndexBuffer);
        CHECK_ERROR(hr);
        Copy(sizeof(g_IndexBufferData), &g_IndexBufferData, pMappedIndexBuffer);
        g_pIndexBuffer->Unmap(0, nullptr);

        g_IndexBufferView.BufferLocation = g_pIndexBuffer->GetGPUVirtualAddress();
        g_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
        g_IndexBufferView.SizeInBytes = indexBufferSize;
    }

    {
        // Now wait until the assets have been uploaded to the GPU
        u64 nextValue = g_FenceValue + 1;
        hr = g_pCommandQueue->Signal(g_pFence, nextValue);
        CHECK_ERROR(hr);

        // now we wait
        if(g_pFence->GetCompletedValue() < nextValue)
        {
            g_pFence->SetEventOnCompletion(nextValue, g_FenceEvent);
            WaitForSingleObject(g_FenceEvent, INFINITE);
        }
    }
}

extern "C"
void
Win32LoadRenderer(Win32Window& window)
{
    HRESULT hr = ERROR_SUCCESS;

    RECT rcClient;
    GetClientRect(window.hWindow, &rcClient);

    g_Uniforms.model = Mat4();
    g_Uniforms.view = Translate(Vec3(0.0f, 0.0f, 2.5f /*zoom*/));
    g_Uniforms.projection = Perspective(45.0f, (f32)(rcClient.right - rcClient.left)/(float)(rcClient.bottom - rcClient.top), 0.01f, 1024.0f);

#ifdef DEBUG_GRAPHICS
    {
        // Always enable the debug layer before doing anything DX12 related
        // so all possible errors generated while creating DX12 objects
        // are caught by the debug layer.
        ID3D12Debug* debugInterface = 0;
        hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
        if(debugInterface) {
            debugInterface->EnableDebugLayer();
        }
        ID3D12Debug1* debugController = 0;
        hr = debugInterface->QueryInterface(IID_PPV_ARGS(&debugController));
        if(debugController)
        {
            debugController->SetEnableGPUBasedValidation(true);
        }
        COM_RELEASE(debugInterface);
        COM_RELEASE(debugController);
        CHECK_ERROR(hr);
    }
#endif
    {
        // Check for variable refresh rate support, IE tearing support

        // Rather than create the DXGI 1.5 factory interface directly, we create the
        // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
        // graphics debugging tools which will not support the 1.5 factory interface 
        // until a future update.
        IDXGIFactory4* factory4 = 0;

        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
        {
            IDXGIFactory5* factory5 = 0;

            if (SUCCEEDED(factory4->QueryInterface(IID_PPV_ARGS(&factory5))))
            {
                BOOL allowTearing = FALSE;
                if (SUCCEEDED(factory5->CheckFeatureSupport(
                        DXGI_FEATURE_PRESENT_ALLOW_TEARING, 
                        &allowTearing, sizeof(allowTearing))))
                {
                    g_tearing_support = allowTearing == TRUE;
                }
            }

            COM_RELEASE(factory5);
        }
        COM_RELEASE(factory4);
    }

    // Now get the DXGI adapter
    IDXGIAdapter4* dxgiAdapter4 = nullptr;
    {
        IDXGIFactory4* dxgiFactory = 0;
        UINT createFactoryFlags = 0;
        
        #if defined(DEBUG_GRAPHICS)
            createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
        #endif

        hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
        CHECK_ERROR(hr);

        IDXGIAdapter1* dxgiAdapter1 = 0;
        
        if (g_use_warp)
        {
            hr = dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1));
            CHECK_ERROR(hr);
            hr = dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4));
            CHECK_ERROR(hr);
            COM_RELEASE(dxgiAdapter1);
        }
        else
        {
            SIZE_T maxDedicatedVideoMemory = 0;
            for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
                dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

                // Check to see if the adapter can create a D3D12 device without actually 
                // creating it. The adapter with the largest dedicated video memory
                // is favored.

                bool supports_d3d12 = true;
                supports_d3d12 &= (dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
                supports_d3d12 &= SUCCEEDED(D3D12CreateDevice(dxgiAdapter1, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr));
                supports_d3d12 &= dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory;

                if ( supports_d3d12 )
                {
                    COM_RELEASE(dxgiAdapter4);
                    maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                    hr = dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4));
                }
                COM_RELEASE(dxgiAdapter1);
            }
        }
        
        COM_RELEASE(dxgiFactory);
    }

    // now create the device
    {
        hr = D3D12CreateDevice(dxgiAdapter4, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&g_pDevice));
        CHECK_ERROR(hr);
        // Enable debug messages in debug mode.

        #if defined(DEBUG_GRAPHICS)
            hr = g_pDevice->QueryInterface(IID_PPV_ARGS(&g_pDebugDevice));
            CHECK_ERROR(hr);

            ID3D12InfoQueue* pInfoQueue = 0;
            if (SUCCEEDED(g_pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue))))
            {
                pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
                pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // Suppress whole categories of messages

                //D3D12_MESSAGE_CATEGORY Categories[] = {};

                // Suppress messages based on their severity level
                D3D12_MESSAGE_SEVERITY Severities[] =
                {
                    D3D12_MESSAGE_SEVERITY_INFO
                };

                // Suppress individual messages by their ID
                D3D12_MESSAGE_ID DenyIds[] = {
                    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
                    D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
                    D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
                };

                D3D12_INFO_QUEUE_FILTER NewFilter = {};
                //NewFilter.DenyList.NumCategories = _countof(Categories);
                //NewFilter.DenyList.pCategoryList = Categories;
                NewFilter.DenyList.NumSeverities = ARRAY_COUNT(Severities);
                NewFilter.DenyList.pSeverityList = Severities;
                NewFilter.DenyList.NumIDs = ARRAY_COUNT(DenyIds);
                NewFilter.DenyList.pIDList = DenyIds;

                hr = pInfoQueue->PushStorageFilter(&NewFilter);
                CHECK_ERROR(hr);

                
            }
            COM_RELEASE(pInfoQueue);
        #endif
    }
    COM_RELEASE(dxgiAdapter4);

    // Now create the command queue
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type =     D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags =    D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;

        hr = g_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pCommandQueue));
        CHECK_ERROR(hr)
    }

    // Swap Chain time...
    {
        IDXGIFactory4* dxgiFactory4 = 0;
        UINT createFactoryFlags = 0;

    #if defined(DEBUG_GRAPHICS)
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
    #endif

        hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4));

        

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = rcClient.right - rcClient.left;   // Backbuffer - window client width
        swapChainDesc.Height = rcClient.bottom - rcClient.top;  // Backbuffer - window client height
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc = { 1, 0 };
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = g_num_frames;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        // It is recommended to always allow tearing if tearing support is available.
        swapChainDesc.Flags = g_tearing_support ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        IDXGISwapChain1* swapChain1 = 0;
        hr = dxgiFactory4->CreateSwapChainForHwnd(
            g_pCommandQueue,
            window.hWindow,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1);
        CHECK_ERROR(hr);

        // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
        // will be handled manually.
        hr = dxgiFactory4->MakeWindowAssociation(window.hWindow, DXGI_MWA_NO_ALT_ENTER);
        CHECK_ERROR(hr);
        hr = swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain));
        CHECK_ERROR(hr);

        COM_RELEASE(swapChain1);
        COM_RELEASE(dxgiFactory4);
    }

    // start off with the correct index
    g_CurrentBackBufferIndex = g_pSwapChain->GetCurrentBackBufferIndex();

    // Create the render target view descriptor heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = g_num_frames;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

        hr = g_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_RTVDescriptorHeap));
        CHECK_ERROR(hr);
    }
    g_RTVDescriptorSize = g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // setup the RTV views
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

        for (int i = 0; i < g_num_frames; ++i)
        {
            ID3D12Resource* backBuffer = 0;
            hr = g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
            CHECK_ERROR(hr);

            g_pDevice->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);
            g_pBackBuffers[i] = backBuffer;
            rtvHandle.ptr += g_RTVDescriptorSize;
        }
    }

    // Create the command allocators
    for(int i = 0; i < g_num_frames; ++i)
    {
        hr = g_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_pCommandAllocators[i]));
        CHECK_ERROR(hr);
    }

    // now the command list
    {
        hr = g_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_pCommandAllocators[g_CurrentBackBufferIndex], nullptr, IID_PPV_ARGS(&g_pCommandList));
        CHECK_ERROR(hr);
        hr = g_pCommandList->Close();
        CHECK_ERROR(hr);
    }

    // create the fence for the queue
    {
        hr = g_pDevice->CreateFence(g_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_pFence));
        CHECK_ERROR(hr);
    }

    g_FenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

    Win32D3d12InitResources();  // Temporary
}

// #ifdef DEBUG_GRAPHICS
// #include <dxgidebug.h>
// typedef GUID DXGI_DEBUG_ID;

// DEFINE_GUID(DXGI_DEBUG_DX, 0x35cdd7fc, 0x13b2, 0x421d, 0xa5, 0xd7, 0x7e, 0x44, 0x51, 0x28, 0x7d, 0x64);
// DEFINE_GUID(DXGI_DEBUG_DXGI, 0x25cddaa4, 0xb1c6, 0x47e1, 0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a);
// DEFINE_GUID(DXGI_DEBUG_APP, 0x6cd6e01, 0x4219, 0x4ebd, 0x87, 0x9, 0x27, 0xed, 0x23, 0x36, 0xc, 0x62);

// DEFINE_GUID(DXGI_DEBUG_D3D11, 0x4b99317b, 0xac39, 0x4aa6, 0xbb, 0xb, 0xba, 0xa0, 0x47, 0x84, 0x79, 0x8f);

// typedef HRESULT (*dxgi_get_debug_interface)(REFIID riid, void   **ppDebug);
// #endif

extern "C"
void
Win32UnloadRenderer()
{
    // Now release everything

    if(g_FenceEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_FenceEvent);
    }
    COM_RELEASE(g_pFence);
    
    COM_RELEASE(g_pCommandList);
    for(int i = 0; i < g_num_frames; ++i)
    {
        COM_RELEASE(g_pBackBuffers[i]);
        COM_RELEASE(g_pCommandAllocators[i]);
    }
    COM_RELEASE(g_RTVDescriptorHeap);
    COM_RELEASE(g_pSwapChain);
    COM_RELEASE(g_pCommandQueue);
    COM_RELEASE(g_pDebugDevice);
    COM_RELEASE(g_pDevice);

// #ifdef DEBUG_GRAPHICS

    // this is from directx samples for memory management techniques
    // https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/TechniqueDemos/D3D12MemoryManagement/src/Framework.cpp
    // IDXGIDebug1* pDebug = nullptr;
    // if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    // {
    //     pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
    //     pDebug->Release();
    // }
//     {
//         HMODULE debugLib = LoadLibraryA("DXGIDebug.dll");

//         if(debugLib)
//         {
//             dxgi_get_debug_interface* DXGIGetDebugInterface = (dxgi_get_debug_interface*)GetProcAddress(debugLib, "DXGIGetDebugInterface");

//             IDXGIDebug* debugInterface = 0;
//             HRESULT hr = (*DXGIGetDebugInterface)(IID_PPV_ARGS(&debugInterface));

//             if(debugInterface) {
//                 DEFINE_GUID(DXGI_DEBUG_ALL, 0xe48ae283, 0xda80, 0x490b, 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8);
//                 debugInterface->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
//             }
//             COM_RELEASE(debugInterface);
//             CHECK_ERROR(hr);

//             FreeLibrary(debugLib);
//         }
//     }
// #endif
}

extern "C"
void
Win32BeginFrame()
{
    // TODO(james): Render the damn triangle!!!
    NotImplemented;
}

extern "C"
void
Win32EndFrame()
{
    HRESULT hr = ERROR_SUCCESS;
    ID3D12CommandAllocator* commandAllocator = g_pCommandAllocators[g_CurrentBackBufferIndex];
    ID3D12Resource* backBuffer = g_pBackBuffers[g_CurrentBackBufferIndex];

    commandAllocator->Reset();
    g_pCommandList->Reset(commandAllocator, nullptr);
        // Clear the render target.
    {

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = backBuffer;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

        g_pCommandList->ResourceBarrier(1, &barrier);
                FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += g_CurrentBackBufferIndex * g_RTVDescriptorSize;

        g_pCommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }
        // Present
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = backBuffer;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        g_pCommandList->ResourceBarrier(1, &barrier);
        hr = g_pCommandList->Close();
        CHECK_ERROR(hr);

        ID3D12CommandList* const commandLists[] = {
            g_pCommandList
        };
        g_pCommandQueue->ExecuteCommandLists(ARRAY_COUNT(commandLists), commandLists);
        UINT syncInterval = g_vsync ? 1 : 0;
        UINT presentFlags = g_tearing_support && !g_vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
        hr = g_pSwapChain->Present(syncInterval, presentFlags);
        CHECK_ERROR(hr);

        u64 nextFenceValue = g_FenceValue + 1;
        hr = g_pCommandQueue->Signal(g_pFence, nextFenceValue);
        CHECK_ERROR(hr);
        g_FrameFenceValues[g_CurrentBackBufferIndex] = nextFenceValue;
        g_CurrentBackBufferIndex = g_pSwapChain->GetCurrentBackBufferIndex();

        if (g_pFence->GetCompletedValue() < nextFenceValue)
        {
            hr = g_pFence->SetEventOnCompletion(nextFenceValue, g_FenceEvent);
            CHECK_ERROR(hr);
            ::WaitForSingleObject(g_FenceEvent, INFINITE);
        }

    }
}

/*******************************************************************************

// Main D3d12 routine order
    
        g_IsInitialized = true;

    ::ShowWindow(g_hWnd, SW_SHOW);

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }
        // Make sure the command queue has finished all commands before closing.
    Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

    ::CloseHandle(g_FenceEvent);

    return 0;
}

=============== HELPERS





void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
    uint64_t& fenceValue, HANDLE fenceEvent )
{
    uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
    WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}


void Resize(uint32_t width, uint32_t height)
{
    if (g_ClientWidth != width || g_ClientHeight != height)
    {
        // Don't allow 0 size swap chain back buffers.
        g_ClientWidth = std::max(1u, width );
        g_ClientHeight = std::max( 1u, height);

        // Flush the GPU queue to make sure the swap chain's back buffers
        // are not being referenced by an in-flight command list.
        Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);
                for (int i = 0; i < g_NumFrames; ++i)
        {
            // Any references to the back buffers must be released
            // before the swap chain can be resized.
            g_BackBuffers[i].Reset();
            g_FrameFenceValues[i] = g_FrameFenceValues[g_CurrentBackBufferIndex];
        }
                DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        ThrowIfFailed(g_SwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(g_SwapChain->ResizeBuffers(g_NumFrames, g_ClientWidth, g_ClientHeight,
            swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);
    }
}

void SetFullscreen(bool fullscreen)
{
    if (g_Fullscreen != fullscreen)
    {
        g_Fullscreen = fullscreen;

        if (g_Fullscreen) // Switching to fullscreen.
        {
            // Store the current window dimensions so they can be restored 
            // when switching out of fullscreen state.
            ::GetWindowRect(g_hWnd, &g_WindowRect);
                        // Set the window style to a borderless window so the client area fills
            // the entire screen.
            UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

            ::SetWindowLongW(g_hWnd, GWL_STYLE, windowStyle);
                        // Query the name of the nearest display device for the window.
            // This is required to set the fullscreen dimensions of the window
            // when using a multi-monitor setup.
            HMONITOR hMonitor = ::MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFOEX monitorInfo = {};
            monitorInfo.cbSize = sizeof(MONITORINFOEX);
            ::GetMonitorInfo(hMonitor, &monitorInfo);
                        ::SetWindowPos(g_hWnd, HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ::ShowWindow(g_hWnd, SW_MAXIMIZE);
        }
                else
        {
            // Restore all the window decorators.
            ::SetWindowLong(g_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

            ::SetWindowPos(g_hWnd, HWND_NOTOPMOST,
                g_WindowRect.left,
                g_WindowRect.top,
                g_WindowRect.right - g_WindowRect.left,
                g_WindowRect.bottom - g_WindowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ::ShowWindow(g_hWnd, SW_NORMAL);
        }
    }
}

********************************************************************************/