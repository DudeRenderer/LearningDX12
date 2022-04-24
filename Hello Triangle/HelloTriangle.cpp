#include "../Common/SampleBase/SampleBase.h"
#include "../Common/SampleBase/d3dUtil.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;


class HelloTriangle : public SampleBase
{
public:
    HelloTriangle(HINSTANCE hInstance);
    ~HelloTriangle();

    virtual bool Initialize() override;

private:
    virtual void OnResize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    virtual void OnDestory() override;


    void PopulateCommandList();
    void WaitForPreviousFrame();

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
    };

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // App resources.
    ComPtr<ID3D12Resource>   m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    // Synchronization objects.
    UINT                m_frameIndex;
    HANDLE              m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64              m_fenceValue;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        HelloTriangle theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}


HelloTriangle::HelloTriangle(HINSTANCE hInstance) :
    SampleBase(hInstance)
{
}

HelloTriangle::~HelloTriangle()
{
}

bool HelloTriangle::Initialize()
{
    if (!SampleBase::Initialize())
        return false;


    //////////////////////////////////////////////////////////////////////////
    // load assets

    // Create an empty root signature.
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(md3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }


    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile((L"../Shaders/shaders.hlsl"), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile((L"../Shaders/shaders.hlsl"), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout                        = {inputElementDescs, _countof(inputElementDescs)};
        psoDesc.pRootSignature                     = m_rootSignature.Get();
        psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable      = FALSE;
        psoDesc.DepthStencilState.StencilEnable    = FALSE;
        psoDesc.SampleMask                         = UINT_MAX;
        psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets                   = 1;
        psoDesc.RTVFormats[0]                      = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count                   = 1;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // Create the command list.
    ThrowIfFailed(md3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pDirectCmdListAlloc.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_pCommandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_pCommandList->Close());

    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
            {
                {{0.0f, 0.25f * AspectRatio(), 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                {{0.25f, -0.25f * AspectRatio(), 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                {{-0.25f, -0.25f * AspectRatio(), 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}};

        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not
        // recommended. Every time the GPU needs it, the upload heap will be marshalled
        // over. Please read up on Default Heap usage. An upload heap is used here for
        // code simplicity and because there are very few verts to actually transfer.
        ThrowIfFailed(md3dDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8*        pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes  = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes    = vertexBufferSize;
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command
        // list in our main loop but for now, we just want to wait for setup to
        // complete before continuing.
        WaitForPreviousFrame();
    }


    return true;
}

void HelloTriangle::OnResize()
{
    SampleBase::OnResize();
}

void HelloTriangle::Update(const GameTimer& gt)
{
}

void HelloTriangle::Draw(const GameTimer& gt)
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = {m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_pSwapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void HelloTriangle::OnDestory()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void HelloTriangle::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated
    // command lists have finished execution on the GPU; apps should use
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_pDirectCmdListAlloc->Reset());

    // However, when ExecuteCommandList() is called on a particular command
    // list, that command list can then be reset at any time and must be before
    // re-recording.
    ThrowIfFailed(m_pCommandList->Reset(m_pDirectCmdListAlloc.Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_pCommandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_pCommandList->RSSetViewports(1, &mScreenViewport);
    m_pCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pRtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_iRtvDescriptorSize);
    m_pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    m_pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_pCommandList->DrawInstanced(3, 1, 0, 0);

    // Indicate that the back buffer will now be used to present.
    m_pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_pCommandList->Close());
}

void HelloTriangle::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_pCommandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}
