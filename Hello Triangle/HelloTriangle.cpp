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

    ComPtr<ID3D12RootSignature> m_pRootSignature;
    ComPtr<ID3D12PipelineState> m_pPipelineState;

    // App resources.
    ComPtr<ID3D12Resource>   m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
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
        ThrowIfFailed(md3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature)));
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
        psoDesc.pRootSignature                     = m_pRootSignature.Get();
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
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));
    }

    // Create the command list.
    ThrowIfFailed(md3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pDirectCmdListAlloc.Get(), m_pPipelineState.Get(), IID_PPV_ARGS(&m_pCommandList)));

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

    ThrowIfFailed(m_pDirectCmdListAlloc->Reset());

    ThrowIfFailed(m_pCommandList->Reset(m_pDirectCmdListAlloc.Get(), m_pPipelineState.Get()));


    m_pCommandList->RSSetViewports(1, &mScreenViewport);
    m_pCommandList->RSSetScissorRects(1, &mScissorRect);

    m_pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
 
    // Indicate a state transition on the resource usage.
    m_pCommandList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), 
                                                                                D3D12_RESOURCE_STATE_PRESENT, 
                                                                                D3D12_RESOURCE_STATE_RENDER_TARGET));


    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, m_iRtvDescriptorSize);
    m_pCommandList->OMSetRenderTargets(1, &rtvHandle, true, &DepthStencilView());

    // Record commands.
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    m_pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_pCommandList->DrawInstanced(3, 1, 0, 0);

    m_pCommandList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                                                                D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                                D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(m_pCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = {m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // swap the back and front buffers
    ThrowIfFailed(m_pSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}

void HelloTriangle::OnDestory()
{

}

