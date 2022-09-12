#define GLFW_EXPOSE_NATIVE_WIN32

#include <iostream>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <vector>
#include "glm/vec3.hpp"

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
    HWND hwnd = glfwGetWin32Window(window);

    // Declare DirectX 12 handles
    IDXGIFactory4* factory;
    ID3D12Debug1* debug_interface;

    // Create factory
    UINT dxgi_factory_flags = 0;

#if _DEBUG
    // If we're in debug mode, create a debug layer for proper error tracking
    ID3D12Debug* debug_layer;
    ID3D12InfoQueue* debug_info_queue;
    throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_layer)));
    throw_if_failed(debug_layer->QueryInterface(IID_PPV_ARGS(&debug_interface)));
    debug_interface->EnableDebugLayer();
    debug_interface->SetEnableGPUBasedValidation(true);
    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    debug_layer->Release();
#endif

    HRESULT result = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory));

    /* ADAPTER:
    * An adapter is used to get information about the GPU like the name, manufacturer,
    * amount of VRAM, etc.
    * We use it here to check if it's a hardware device that supports Direct3D 12.0
    */

    // Create the device handle
    ID3D12Device* device = nullptr;

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

        // We should have a hardware adapter now, but does it support Direct3D 12.0?
        if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
            // Yes it does! We use this one.
            break;
        }

        // It doesn't? Unfortunate, let it go and try another adapter
        device = nullptr;
        adapter->Release();
        adapter_index++;
    }

    if (device == nullptr) {
        throw std::exception();
    }

    /* DEVICE:
    * A device is our main access to the DirectX 12 API, data structures, rendering functions, etc.
    */

#if _DEBUG
    // If we're in debug mode, create the debug device handle
    ID3D12DebugDevice* device_debug;
    throw_if_failed(device->QueryInterface(&device_debug));
#endif

    /* COMMAND LIST:
    * A command list is a group of draw calls or a list of functions that you want the GPU to run
    */

    /* COMMAND QUEUE:
    * A command queue is a queue that you can submit command lists to.
    */

    // Create command queue
    ID3D12CommandQueue* command_queue;
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    throw_if_failed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));

    /* COMMAND ALLOCATOR:
    * A command allocator is used to create command lists
    */
    ID3D12CommandAllocator* command_allocator;
    throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                                   IID_PPV_ARGS(&command_allocator)));

    /* FENCE:
    * A fence is used for synchronization between CPU and GPU, and lets you know when the GPU is done
    * with its tasks (e.g. uploads, rendering,) so you can send more commands.
    */

    // Create fence
    UINT frame_index;
    HANDLE fence_event;
    ID3D12Fence* fence;
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

    // Create fullscreen description
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {};

    // Create swapchain
    IDXGISwapChain1* new_swapchain;
    result = factory->CreateSwapChainForHwnd(command_queue, hwnd, &swapchain_desc, 
                                    nullptr, nullptr, &new_swapchain);
    HRESULT swapchain_support = new_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), 
                                                                (void**)&new_swapchain);
    if (SUCCEEDED(swapchain_support)) {
        swapchain = (IDXGISwapChain3*)new_swapchain;
    }

    if (!swapchain) {
        throw std::exception();
    }

    frame_index = swapchain->GetCurrentBackBufferIndex();

    /* RESOURCE
    * Simply just a chunk of memory on the GPU
    */

    /* DESCRIPTOR
    * Describes said chunk of memory, and how it should be interpreted
    * Examples of descriptor types: Render Target View (RTV), Shader Resource View (SRV)
    * Depth Stencil View (DSV), Constant Buffer View (CBV), Unordered Access View (UAV)
    */

    /* DESCRIPTOR HEAP
    * A bunch of resource descriptors in a row
    */

    // Create render target view descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
    rtv_heap_desc.NumDescriptors = backbuffer_count;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    throw_if_failed(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&render_target_view_heap)));

    rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create frame resources
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle(render_target_view_heap->GetCPUDescriptorHandleForHeapStart());

    // Create RTV for each frame
    for (UINT i = 0; i < backbuffer_count; i++) {
        throw_if_failed(swapchain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
        device->CreateRenderTargetView(render_targets[i], nullptr, rtv_handle);
        rtv_handle.ptr += (1 * rtv_descriptor_size);
    }

    /* ROOT SIGNATURE
    * A root signature is an object that defines which resource parameters your
    * shaders have access to, like constant buffers, structured buffers, textures and samplers
    */

    ID3D12RootSignature* root_signature;
    D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
    feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data)))) {
        feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    /* DESCRIPTOR RANGE
    * Describes a chunk of descriptors in a row, saying what type of descriptors it holds, 
    * how many descriptors it holds, the offset, etc.
    */

    // We want one constant buffer resource on the GPU, so add it to the descriptor ranges
    D3D12_DESCRIPTOR_RANGE1 ranges[1];
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors = 1;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;
    ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

    /* ROOT PARAMETER
    * Determines the shader visibility, the type of parameter, the number of descriptor ranges, 
    * and holds a pointer to an array of ranges
    */

    // Bind the descriptor ranges to the descriptor table of the root signature
    D3D12_ROOT_PARAMETER1 root_parameters[1];
    root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    root_parameters[0].DescriptorTable.NumDescriptorRanges = 1;
    root_parameters[0].DescriptorTable.pDescriptorRanges = ranges;

    /* ROOT SIGNATURE DESCRIPTION
    * Determines the number of parameters, the number of samplers, and holds pointers to 
    * said parameters and samplers.
    */

    // Create a root signature description
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    root_signature_desc.Desc_1_1.NumParameters = 1;
    root_signature_desc.Desc_1_1.pParameters = root_parameters;
    root_signature_desc.Desc_1_1.NumStaticSamplers = 0;
    root_signature_desc.Desc_1_1.pStaticSamplers = nullptr;

    /* EXTRA NOTES:
    * A root signature has a root signature description.
    * A root signature description has root parameters and samplers.
    * A root parameter has descriptor ranges.
    * A descriptor range has a bunch of descriptors.
    * A descriptor determines how a resource should be used.
    * So: root signature description -> root params[] -> descriptor range[] -> descriptor[] -> resource
    */

    // Now let's create a root signature
    ID3DBlob* signature;
    ID3DBlob* error;
    try {
        throw_if_failed(D3D12SerializeVersionedRootSignature(&root_signature_desc, &signature, &error));
        throw_if_failed(device->CreateRootSignature(0, signature->GetBufferPointer(), 
                        signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
        root_signature->SetName(L"Hello Triangle Root Signature");
    }
    catch (std::exception e) {
        const char* errStr = (const char*)error->GetBufferPointer();
        std::cout << errStr;
        error->Release();
        error = nullptr;
    }

    if (signature) {
        signature->Release();
        signature = nullptr;
    }

    /* HEAP
    * A heap is a sort of gateway to GPU memory, which you can use to upload buffers or 
    * textures to the GPU.
    */

    // Vertex struct
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
    };

    // The vertex buffer we'll display to the screen
    Vertex triangle_verts[] = {
        {{+1.f, -1.f, 0.f}, {1.f, 0.f, 0.f }},
        {{-1.f, -1.f, 0.f}, {0.f, 1.f, 0.f }},
        {{ 0.f, +1.f, 0.f}, {0.f, 0.f, 1.f }},
    };
    uint32_t triangle_indices[] = {
        1,
        2,
        3
    };

    /* VERTEX BUFFER VIEW
    * Similar to a VAO in OpenGL. It has the GPU address of the buffer, the size of the buffer, and the stride
    * of each vertex entry of the buffer.
    */

    // Declare handles
    ID3D12Resource* vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;

    // Only the GPU needs this data, the CPU won't need this
    D3D12_RANGE vertex_range{ 0, 0 };
    uint8_t* vertex_data_begin = nullptr;

    // Upload vertex buffer to GPU
    {
        D3D12_HEAP_PROPERTIES upload_heap_props = {
            D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

        D3D12_RESOURCE_DESC upload_buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
            0,
            sizeof(triangle_verts),
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
            {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
            D3D12_RESOURCE_FLAG_NONE,
        };


        throw_if_failed(device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
            ((void**)&vertex_buffer)));

        // Bind the vertex buffer, copy the data to it, then unbind the vertex buffer
        throw_if_failed(vertex_buffer->Map(0, &vertex_range, (void**)&vertex_data_begin));
        memcpy_s(vertex_data_begin, sizeof(triangle_verts), triangle_verts, sizeof(triangle_verts));
        vertex_buffer->Unmap(0, nullptr);

        // Init the buffer view
        vertex_buffer_view = D3D12_VERTEX_BUFFER_VIEW{
            vertex_buffer->GetGPUVirtualAddress(),
            sizeof(Vertex),
            sizeof(triangle_verts)
        }; 
    }

    /* INDEX BUFFER VIEW
    * Same idea as vertex buffer view, except the data integers
    */

    // Declare handles
    ID3D12Resource* index_buffer;
    D3D12_INDEX_BUFFER_VIEW index_buffer_view;

    // Only the GPU needs this data, the CPU won't need this
    D3D12_RANGE index_range{ 0, 0 };
    uint8_t* index_data_begin = nullptr;

    // Upload vertex buffer to GPU
    {
        D3D12_HEAP_PROPERTIES upload_heap_props = {
            D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

        D3D12_RESOURCE_DESC upload_buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
            0,
            sizeof(triangle_indices),
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
            {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
            D3D12_RESOURCE_FLAG_NONE,
        };


        throw_if_failed(device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
            ((void**)&index_buffer)));

        // Bind the vertex buffer, copy the data to it, then unbind the vertex buffer
        throw_if_failed(index_buffer->Map(0, &index_range, (void**)&index_data_begin));
        memcpy_s(index_data_begin, sizeof(triangle_indices), triangle_indices, sizeof(triangle_indices));
        index_buffer->Unmap(0, nullptr);

        // Init the buffer view
        index_buffer_view = D3D12_INDEX_BUFFER_VIEW{
            index_buffer->GetGPUVirtualAddress(),
            sizeof(Vertex),
            DXGI_FORMAT_R32_UINT,
        };
    }

    /* CONSTANT BUFFER
    * Same as uniform buffers in OpenGL, usually meant to hold transform matrices, initialized 
    * the same way as the other buffers. In this case I will use it to make the triangle pulsate.
    */

    // Define what the constant buffer's layout is
    struct {
        glm::vec3 color_mul;
    } const_buffer_data_struct;

    // Declare handles
    ID3D12Resource* const_buffer;
    ID3D12DescriptorHeap* const_buffer_heap = nullptr;
    D3D12_CONSTANT_BUFFER_VIEW_DESC const_buffer_view_desc = {};

    // Only the GPU needs this data, the CPU won't need this
    D3D12_RANGE const_range{ 0, 0 };
    uint8_t* const_data_begin = nullptr;

    // Upload constant buffer to GPU
    {
        D3D12_HEAP_PROPERTIES upload_heap_props = {
            D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            1,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        };

        throw_if_failed(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&const_buffer_heap)));

        D3D12_RESOURCE_DESC upload_buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
            0,
            (sizeof(const_buffer_data_struct) | 0xFF) + 1, // Constant buffers must be 256-byte aligned
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
            {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
            D3D12_RESOURCE_FLAG_NONE,
        };


        throw_if_failed(device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
            ((void**)&const_buffer)));

        assert(const_buffer_heap != nullptr);
        const_buffer_heap->SetName(L"Constant Buffer Upload Resource Heap");

        // Create the buffer view
        const_buffer_view_desc = {
            const_buffer->GetGPUVirtualAddress(),
            (sizeof(const_buffer_data_struct) | 0xFF) + 1
        };

        D3D12_CPU_DESCRIPTOR_HANDLE const_buffer_view_handle(const_buffer_heap->GetCPUDescriptorHandleForHeapStart());
        device->CreateConstantBufferView(&const_buffer_view_desc, const_buffer_view_handle);

        // Bind the vertex buffer, copy the data to it, then unbind the vertex buffer
        throw_if_failed(const_buffer->Map(0, &const_range, (void**)&const_data_begin));
        memcpy_s(const_data_begin, sizeof(triangle_indices), triangle_indices, sizeof(triangle_indices));
        const_buffer->Unmap(0, nullptr);
    }

    /* SHADERS
    * Shaders are loaded as pre-compiled binary files. Shaders are compiled using the Microsoft DirectX Shader
    * Compiler (https://github.com/microsoft/DirectXShaderCompiler), which compiles .hlsl files into .dxil files.
    */




    // Main window update loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glfwSwapBuffers(window);
    }
}