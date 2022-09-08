#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE

#include <iostream>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>

void throw_if_failed(HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}

constexpr uint32_t width = 1280;
constexpr uint32_t height = 720;
static const UINT backbuffer_count = 2;

int main()
{
    // Create window - use GLFW_NO_API, since we're not using OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Hello Triangle (DirectX 12)", nullptr, nullptr);

    // Main window update loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    // Declare DirectX 12 handles
    IDXGIFactory4* factory;
    ID3D12Debug1* debug_interface;

    // Create factory
    UINT dxgiFactoryFlags = 0;

#if _DEBUG
    // If we're in debug mode, create a debug layer for proper error tracking
    {
        ID3D12Debug* debug_layer;
        throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_layer)));
        throw_if_failed(debug_layer->QueryInterface(IID_PPV_ARGS(&debug_interface)));
        debug_interface->EnableDebugLayer();
        debug_interface->SetEnableGPUBasedValidation(true);
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        debug_layer->Release();
    }
#endif

    HRESULT result = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));

    /* ADAPTER:
    * An adapter is used to get information about the GPU like the name, manufacturer,
    * amount of VRAM, etc.
    * We use it here to check if it's a hardware device that supports Direct3D
    */

    // Create adapter
    IDXGIAdapter1* adapter;
    UINT adapter_index = 0;
    while (factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Ignore software renderer - we want a hardware adapter
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // We should have a hardware adapter now, but does it support Direct3D 12?
        if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr))) {
            // Yes it does! We use this one.
            break;
        }

        // It doesn't? Unfortunate, let it go and try another adapter
        adapter->Release();
        adapter_index++;
    }

    /* DEVICE:
    * A device is our main access to the DirectX 12 API, data structures, rendering functions, etc.
    */

    // Create the device handle
    ID3D12Device* device;
    throw_if_failed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

#if _DEBUG
    // If we're in debug mode, create the debug device handle
    ID3D12DebugDevice * device_debug;
    throw_if_failed(device->QueryInterface(&device_debug));
#endif

    /* COMMAND LIST:
    * A command list is a group of draw calls or a list of functions that you want the GPU to run
    */

    /* COMMAND QUEUE:
    * A command queue is a queue that you can submit command lists to.
    */

    // Create command queue
    ID3D12CommandQueue * command_queue;
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    throw_if_failed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));

    /* COMMAND ALLOCATOR:
    * A command allocator is used to create command lists
    */
    ID3D12CommandAllocator * command_allocator;
    throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));

    /* FENCE:
    * A fence is used for synchronization between CPU and GPU, and lets you know when the GPU is done
    * with its tasks (e.g. uploads, rendering,) so you can send more commands.
    */

    // Create fence
    UINT frame_index;
    HANDLE fence_event;
    ID3D12Fence * fence;
    UINT64 fence_value;
    throw_if_failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    /* BARRIER:
    * A barrier is used for resources, to determine how the driver should access it.
    */

    /* SWAPCHAIN:
    * A swapchain is used to allocate the backbuffers that will be rendered to, and handles
    * swapping between the different backbuffers.
    */

    // Declare variables we need for swapchain
    UINT curr_buffer;
    ID3D12DescriptorHeap* render_target_view_heap;
    ID3D12Resource* render_targets[backbuffer_count];
    UINT rtv_descriptor_size;
    IDXGISwapChain3* swapchain = nullptr;
    D3D12_VIEWPORT viewport;
    D3D12_RECT surface_size;

    // Define surface size
    surface_size.left = 0;
    surface_size.top = 0;
    surface_size.right = width;
    surface_size.bottom = height;

    // Define viewport
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // Create swapchain description
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
    swapchain_desc.BufferCount = backbuffer_count;
    swapchain_desc.Width = width;
    swapchain_desc.Height = height;
    swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.SampleDesc.Count = 1;

    // Create swapchain
    IDXGISwapChain1* new_swapchain;
    factory->CreateSwapChainForHwnd(device, glfwGetWin32Window(window), &swapchain_desc, nullptr, nullptr, &new_swapchain);
    HRESULT swapchain_support = new_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&new_swapchain);
    if (SUCCEEDED(swapchain_support)) {
        swapchain = (IDXGISwapChain3*)new_swapchain;
    }

    if (!swapchain) {
        throw std::exception();
    }

}