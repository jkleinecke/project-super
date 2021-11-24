
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>    // internal only? Compilation should be done offline - Requires d3dcompiler.lib and a DLL

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

********************************************************************************/

// number of buffer frames 1 - Active, 2 Flip-Flop
const u8 g_num_frames = 3;

// actual d3d12 objects
// TODO(james): Check MSDN docs to verify that these are the correct interface versions, ID3D12Device9?
global ID3D12Device2* g_pDevice;
global ID3D12CommandQueue* g_pCommandQueue;
global IDXGISwapChain4* g_pSwapChain;
global ID3D12Resource* g_pBackBuffers[g_num_frames];
global ID3D12GraphicsCommandList* g_pCommandList;
global ID3D12CommandAllocator* g_pCommandAllocators[g_num_frames];
global ID3D12DescriptorHeap* g_RTVDescriptorHeap;
global UINT g_RTVDescriptorSize;
global UINT g_CurrentBackBufferIndex;

// sync objects
global ID3D12Fence* g_pFence;
global u64 g_FenceValue = 0;
global u64 g_FrameFenceValues[g_num_frames] = {};
global HANDLE g_FenceEvent;

// config options
global bool g_use_warp = false;
global bool g_vsync = true;
global bool g_tearing_support = false;
global bool g_fullscreen = false;

#define CHECK_ERROR(hres) if(FAILED(hres)) { LOG_ERROR("D3D12 ERROR: %X", hres); }

extern "C"
void
Win32LoadRenderer(Win32Window& window)
{
    HRESULT hr = ERROR_SUCCESS;

#ifdef DEBUG_GRAPHICS
    {
        // Always enable the debug layer before doing anything DX12 related
        // so all possible errors generated while creating DX12 objects
        // are caught by the debug layer.
        ID3D12Debug* debugInterface = 0;
        D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));


        hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
        if(debugInterface) {
            debugInterface->EnableDebugLayer();
        }
        COM_RELEASE(debugInterface);
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
            IDXGIFactory5* factory5;

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

        IDXGIAdapter1* dxgiAdapter1;
        
        if (g_use_warp)
        {
            hr = dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1));
            CHECK_ERROR(hr);
            hr = dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4));
            CHECK_ERROR(hr);
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
                supports_d3d12 &= SUCCEEDED(D3D12CreateDevice(dxgiAdapter1, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
                supports_d3d12 &= dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory;

                if ( supports_d3d12 )
                {
                    maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                    hr = dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4));
                }
            }
        }

        COM_RELEASE(dxgiAdapter1);
        COM_RELEASE(dxgiFactory);
    }

    // now create the device
    {
        hr = D3D12CreateDevice(dxgiAdapter4, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_pDevice));
        CHECK_ERROR(hr);
        // Enable debug messages in debug mode.

        #if defined(DEBUG_GRAPHICS)
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

        RECT rcClient;
        GetClientRect(window.hWindow, &rcClient);

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
}

extern "C"
void
Win32BeginFrame()
{

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