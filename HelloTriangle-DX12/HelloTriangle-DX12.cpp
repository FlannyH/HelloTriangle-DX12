#define GLFW_EXPOSE_NATIVE_WIN32

#include <iostream>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <vector>
#include "glm/vec3.hpp"
#include <fstream>

void throw_if_failed(HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}

constexpr uint32_t width = 1280;
constexpr uint32_t height = 720;
static const UINT backbuffer_count = 2;

void read_file(const std::string& path, int& size_bytes, char*& data, const bool silent);

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
    HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ID3D12Fence* fences[backbuffer_count];
    UINT64 fence_values[backbuffer_count];
    for (int i = 0; i < backbuffer_count; ++i) {
        throw_if_failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i])));
        fence_values[i] = 0;
    }

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
    UINT render_target_view_descriptor_size;
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
    D3D12_DESCRIPTOR_HEAP_DESC render_target_view_heap_desc{};
    render_target_view_heap_desc.NumDescriptors = backbuffer_count;
    render_target_view_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    render_target_view_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    throw_if_failed(device->CreateDescriptorHeap(&render_target_view_heap_desc, IID_PPV_ARGS(&render_target_view_heap)));

    render_target_view_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create frame resources
    D3D12_CPU_DESCRIPTOR_HANDLE render_target_view_handle(render_target_view_heap->GetCPUDescriptorHandleForHeapStart());

    // Create RTV for each frame
    for (UINT i = 0; i < backbuffer_count; i++) {
        throw_if_failed(swapchain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
        device->CreateRenderTargetView(render_targets[i], nullptr, render_target_view_handle);
        render_target_view_handle.ptr += (1 * render_target_view_descriptor_size);
    }

    /* ROOT SIGNATURE
    * A root signature is an object that defines which resource parameters your
    * shaders have access to, like constant buffers, structured buffers, textures and samplers
    */

    ID3D12RootSignature* root_signature = nullptr;
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
        {{+0.5f, -0.5f, 0.f}, {1.f, 0.f, 0.f }},
        {{-0.5f, -0.5f, 0.f}, {0.f, 1.f, 0.f }},
        {{ 0.0f, +0.5f, 0.f}, {0.f, 0.f, 1.f }},
    };
    uint32_t triangle_indices[] = {
        0,
        1,
        2
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
            sizeof(triangle_verts),
            sizeof(Vertex),
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

    // Upload index buffer to GPU
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

        // Bind the index buffer, copy the data to it, then unbind the index buffer
        throw_if_failed(index_buffer->Map(0, &index_range, (void**)&index_data_begin));
        memcpy_s(index_data_begin, sizeof(triangle_indices), triangle_indices, sizeof(triangle_indices));
        index_buffer->Unmap(0, nullptr);

        // Init the buffer view
        index_buffer_view = D3D12_INDEX_BUFFER_VIEW{
            index_buffer->GetGPUVirtualAddress(),
            sizeof(triangle_indices),
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

    // Load vertex shader
    std::string file_to_load = "Assets/Shaders/DX12/hello_triangle";
    D3D12_SHADER_BYTECODE vs_bytecode{};
    D3D12_SHADER_BYTECODE ps_bytecode{};
    std::string vs_path = file_to_load + ".vs.cso";
    std::string ps_path = file_to_load + ".ps.cso";
    int vs_size = 0;
    int ps_size = 0;
    char* vs_data = nullptr;
    char* ps_data = nullptr;
    read_file(vs_path, vs_size, vs_data, false);
    read_file(ps_path, ps_size, ps_data, false);
    vs_bytecode.BytecodeLength = vs_size;
    ps_bytecode.BytecodeLength = ps_size;
    vs_bytecode.pShaderBytecode = vs_data;
    ps_bytecode.pShaderBytecode = ps_data;

    /* PIPELINE STATE
    * The pipeline state has all the info you need to execute a draw call
    */

    // Define graphics pipeline
    ID3D12PipelineState* pipeline_state;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};

    // Define input assembly - this defines what our shader input is
    D3D12_INPUT_ELEMENT_DESC input_element_descs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    pipeline_state_desc.InputLayout = { input_element_descs, _countof(input_element_descs) };

    // Assign root signature
    pipeline_state_desc.pRootSignature = root_signature;

    // Bind shaders
    pipeline_state_desc.VS = vs_bytecode;
    pipeline_state_desc.PS = ps_bytecode;

    // Set up rasterizer description
    D3D12_RASTERIZER_DESC raster_desc;
    raster_desc.FillMode = D3D12_FILL_MODE_SOLID;
    raster_desc.CullMode = D3D12_CULL_MODE_NONE;
    raster_desc.FrontCounterClockwise = FALSE;
    raster_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    raster_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    raster_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    raster_desc.DepthClipEnable = TRUE;
    raster_desc.MultisampleEnable = FALSE;
    raster_desc.AntialiasedLineEnable = FALSE;
    raster_desc.ForcedSampleCount = 0;
    raster_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    pipeline_state_desc.RasterizerState = raster_desc;
    pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // Setup color and alpha blend modes
    D3D12_BLEND_DESC blend_desc{};
    blend_desc.AlphaToCoverageEnable = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC default_render_target_blend_desc = {
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

    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        blend_desc.RenderTarget[i] = default_render_target_blend_desc;

    pipeline_state_desc.BlendState = blend_desc;


    // Set up depth/stencil state
    pipeline_state_desc.DepthStencilState.DepthEnable = FALSE;
    pipeline_state_desc.DepthStencilState.StencilEnable = FALSE;
    pipeline_state_desc.SampleMask = UINT_MAX;

    // Setup render target output
    pipeline_state_desc.NumRenderTargets = 1;
    pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipeline_state_desc.SampleDesc.Count = 1;

    // Create graphics pipeline state
    try {
        throw_if_failed(device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&pipeline_state)));
    }
    catch (std::exception e) {
        puts("Failed to create Graphics Pipeline");
    }

    // Create command allocator and command list
    ID3D12PipelineState* initial_pipeline_state = nullptr;
    ID3D12GraphicsCommandList* command_list;
    throw_if_failed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator, pipeline_state, IID_PPV_ARGS(&command_list)));


    // Main window update loop
    while (!glfwWindowShouldClose(window)) {
        puts("Start of frame");
        // Bind root signature
        command_list->SetGraphicsRootSignature(root_signature);

        // Bind constant buffer
        ID3D12DescriptorHeap* descriptor_heaps[] = { const_buffer_heap };
        command_list->SetDescriptorHeaps(_countof(descriptor_heaps), descriptor_heaps);
        D3D12_GPU_DESCRIPTOR_HANDLE const_buffer_view_handle(const_buffer_heap->GetGPUDescriptorHandleForHeapStart());
        
        // Set root descriptor table
        command_list->SetGraphicsRootDescriptorTable(0, const_buffer_view_handle);

        // Set backbuffer as render target
        D3D12_RESOURCE_BARRIER render_target_barrier;
        render_target_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        render_target_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        render_target_barrier.Transition.pResource = render_targets[frame_index];
        render_target_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        render_target_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        render_target_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list->ResourceBarrier(1, &render_target_barrier);

        // Set render target
        D3D12_CPU_DESCRIPTOR_HANDLE render_target_view_handle(render_target_view_heap->GetCPUDescriptorHandleForHeapStart());
        render_target_view_handle.ptr += frame_index * render_target_view_descriptor_size;
        command_list->OMSetRenderTargets(1, &render_target_view_handle, FALSE, nullptr);

        // Record raster commands
        const float clear_color[] = {0.1f, 0.1f, 0.2f, 1.0f};
        command_list->RSSetViewports(1, &viewport); // Set viewport
        command_list->RSSetScissorRects(1, &surface_size); // todo: comment
        command_list->ClearRenderTargetView(render_target_view_handle, clear_color, 0, nullptr); // Clear the screen
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // We draw triangles
        command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view); // Bind vertex buffer
        command_list->IASetIndexBuffer(&index_buffer_view); // Bind index buffer
        
        // Submit draw call
        command_list->DrawIndexedInstanced(3, 1, 0, 0, 0);

        // Present backbuffer
        D3D12_RESOURCE_BARRIER present_barrier;
        present_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        present_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        present_barrier.Transition.pResource = render_targets[frame_index];
        present_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        present_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        present_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list->ResourceBarrier(1, &present_barrier);

        // Finish the command list, we're done with the frame
        throw_if_failed(command_list->Close());

        // Execute command list
        ID3D12CommandList* command_lists[] = { command_list };
        command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

        // Present
        swapchain->Present(1, 0);

        // Use fence to wait until the frame is fully rendered
        throw_if_failed(command_queue->Signal(fences[frame_index], fence_values[frame_index]));

        if (fences[frame_index]->GetCompletedValue() < fence_values[frame_index]) {
            throw_if_failed(fences[frame_index]->SetEventOnCompletion(fence_values[frame_index], fence_event));
            WaitForSingleObject(fence_event, INFINITE);
        }
        fence_values[frame_index]++;

        frame_index = swapchain->GetCurrentBackBufferIndex();

        // Update GLFW window
        glfwPollEvents();
        glfwSwapBuffers(window);

        // Reset command allocator and use the raster graphics pipeline
        throw_if_failed(command_allocator->Reset());
        throw_if_failed(command_list->Reset(command_allocator, pipeline_state));
    }

    /* TODO
    * // TO FIX THE CODE
    * - Split each step into its own function
    * - Dont Repeat Yourself
    * - Maybe just pop this into the FlanRenderer-RW header, it was made for cross-api stuff so might as well
    */
}

void read_file(const std::string& path, int& size_bytes, char*& data, const bool silent)
{
    //Open file
    std::ifstream file_stream(path, std::ios::binary);

    //Is it actually open?
    if (file_stream.is_open() == false)
    {
        if (!silent)
            printf("[ERROR] Failed to open file '%s'!\n", path.c_str());
        size_bytes = 0;
        data = nullptr;
        return;
    }

    //See how big the file is so we can allocate the right amount of memory
    const auto begin = file_stream.tellg();
    file_stream.seekg(0, std::ifstream::end);
    const auto end = file_stream.tellg();
    const auto size = end - begin;
    size_bytes = size;

    //Allocate memory
    data = static_cast<char*>(malloc(static_cast<uint32_t>(size)));

    //Load file data into that memory
    file_stream.seekg(0, std::ifstream::beg);
    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(file_stream), {});

    //Is it actually open?
    if (buffer.empty())
    {
        if (!silent)
            printf("[ERROR] Failed to open file '%s'!\n", path.c_str());
        size_bytes = 0;
        data = nullptr;
        return;
    }
    memcpy(data, &buffer[0], size_bytes);
}
