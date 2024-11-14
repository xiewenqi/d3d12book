//***************************************************************************************
// BoxApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Shows how to draw a box in Direct3D 12.
//
// Controls:
//   Hold the left mouse button down and move the mouse to rotate.
//   Hold the right mouse button down and move the mouse to zoom in and out.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#define USING_SINGLE_VERTEX_BUFFER 1

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

struct VertexPosData
{
    XMFLOAT3 Pos;
};

struct VertexColorData
{
    XMFLOAT4 Color;
};

struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
    float CurrentTime;
};

struct GlobalConstants
{
    float GlobalTime;
};

class BoxApp : public D3DApp
{
public:
	BoxApp(HINSTANCE hInstance);
    BoxApp(const BoxApp& rhs) = delete;
    BoxApp& operator=(const BoxApp& rhs) = delete;
	~BoxApp();

	virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void BuildDescriptorHeaps();
	void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
#if USING_SINGLE_VERTEX_BUFFER
    void BuildBoxGeometry();
#else
    void BuildBoxGeometryMultiVertexBuffers();
#endif
    void BuildPyramidGeometry();
    
    void BuildPSO();

private:
    
    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

#if USING_SINGLE_VERTEX_BUFFER
	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
#else
    ComPtr<ID3D12Resource> mVertexBufferPosGPU = nullptr;
    ComPtr<ID3D12Resource> mVertexBufferPosUploader = nullptr;
    UINT mVertexBufferPosSize = 0;
    
    ComPtr<ID3D12Resource> mVertexBufferColorGPU = nullptr;
    ComPtr<ID3D12Resource> mVertexBufferColorUploader = nullptr;
    UINT mVertexBufferColorSize = 0;

    ComPtr<ID3D12Resource> mIndexBufferGPU = nullptr;
    ComPtr<ID3D12Resource> mIndexBufferUploader = nullptr;
    UINT mIndexBufferSize = 0;
    UINT mIndexCount = 0;
#endif

    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    XMFLOAT4X4 mWorldForPyramid = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = XM_PIDIV4;
    float mRadius = 6.0f;

    const int BoxVertexCount = 8;
    const int BoxIndexCount = 36;

    const int PyramidVertexCount = 5;
    const int PyramidIndexCount = 18;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
				   PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    try
    {
        BoxApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

BoxApp::BoxApp(HINSTANCE hInstance)
: D3DApp(hInstance) 
{
}

BoxApp::~BoxApp()
{
}

bool BoxApp::Initialize()
{
    if(!D3DApp::Initialize())
		return false;
		
    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
 
    BuildDescriptorHeaps();
	BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
#if USING_SINGLE_VERTEX_BUFFER
    BuildBoxGeometry();
#else
    BuildBoxGeometryMultiVertexBuffers();
#endif
    BuildPSO();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

	return true;
}

void BoxApp::OnResize()
{
	D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void BoxApp::Update(const GameTimer& gt)
{
    // Convert Spherical to Cartesian coordinates.
    float x = mRadius*sinf(mPhi)*cosf(mTheta);
    float z = mRadius*sinf(mPhi)*sinf(mTheta);
    float y = mRadius*cosf(mPhi);

    // Build the view matrix.
    XMVECTOR viewOffset = XMVectorSet(-1.5, 0, 0, 0);
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f) + viewOffset;
    XMVECTOR target = XMVectorZero() + viewOffset;
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    
    // 立方体设置constant buffer值
    {

        XMMATRIX worldViewProj = world * view * proj;

        // Update the constant buffer with the latest worldViewProj matrix.
        ObjectConstants objConstants;
        XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
        objConstants.CurrentTime = mTimer.TotalTime();
        mObjectCB->CopyData(0, objConstants);
    }

    // 金字塔设置constant buffer值
    {
        XMMATRIX worldForPyramid = XMMatrixTranslation(-3.0f, 0, 0);
        XMMATRIX wvp = worldForPyramid * view * proj;

        ObjectConstants objConstants;
        XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(wvp));
        objConstants.CurrentTime = mTimer.TotalTime();
        mObjectCB->CopyData(1, objConstants);
    }
}

void BoxApp::Draw(const GameTimer& gt)
{
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    // todo xiewneqi: ID3D12GraphicsCommandList::Reset中提供了ID3D12PipelineState参数时，就不用再调用ID3D12GraphicsCommandList::SetPipelineState了？
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // 绘制立方体
    mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());
#if USING_SINGLE_VERTEX_BUFFER
	mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
    mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
    SubmeshGeometry boxSubGeo = mBoxGeo->DrawArgs["box"];
    mCommandList->DrawIndexedInstanced(
		boxSubGeo.IndexCount, 1, boxSubGeo.StartIndexLocation, boxSubGeo.BaseVertexLocation, 0);
#else
    // 位置顶点缓冲区与颜色顶点缓冲区视图
    D3D12_VERTEX_BUFFER_VIEW posAndColorBufferViews[2] = {
        {
            mVertexBufferPosGPU->GetGPUVirtualAddress(),
            mVertexBufferPosSize,
            sizeof(VertexPosData)
        },
        {
            mVertexBufferColorGPU->GetGPUVirtualAddress(),
            mVertexBufferColorSize,
            sizeof(VertexColorData)
        },
    };
    mCommandList->IASetVertexBuffers(0, 2, posAndColorBufferViews);

        // 索引缓冲区视图
    D3D12_INDEX_BUFFER_VIEW indexBufferView = {
        mIndexBufferGPU->GetGPUVirtualAddress(),
        mIndexBufferSize,
        DXGI_FORMAT_R16_UINT
    };
    mCommandList->IASetIndexBuffer(&indexBufferView);

    mCommandList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
#endif

    // 绘制金字塔，此时VB/IB与立方体是共享的，但wvp矩阵常量缓冲区仍然单独指定
    CD3DX12_GPU_DESCRIPTOR_HANDLE pyrimidViewHandle(mCbvHeap->GetGPUDescriptorHandleForHeapStart(), 1, mCbvSrvUavDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(0, pyrimidViewHandle);
    SubmeshGeometry pyramidSubGeo = mBoxGeo->DrawArgs["pyramid"];
    mCommandList->DrawIndexedInstanced(pyramidSubGeo.IndexCount, 1, pyramidSubGeo.StartIndexLocation, pyramidSubGeo.BaseVertexLocation, 0);
	
    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
	ThrowIfFailed(mCommandList->Close());
 
    // Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	
	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.005 unit in the scene.
        float dx = 0.005f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

/**
 * 构建常量缓冲区描述符堆。
 */
void BoxApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 2;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap)));
}

/**
 * 创建常量缓冲对象（ID3D12Resource）及关联的ConstantBufferView；
 */
void BoxApp::BuildConstantBuffers()
{
    // 创建object constant buffer及描述符
    {
        // 创建两个上传堆及对应的描述符，一个给立方体，一个给金字塔
        UINT numberOfConstantBuffer = 2;
        mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), numberOfConstantBuffer, true);

        UINT objCBByteSize = mObjectCB->ElementByteSize();

        D3D12_GPU_VIRTUAL_ADDRESS cbBaseAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
        // Offset to the ith object constant buffer in the buffer.
        for (UINT32 cbIndex = 0; cbIndex < numberOfConstantBuffer; ++cbIndex)
        {
            D3D12_GPU_VIRTUAL_ADDRESS cbAddress = cbBaseAddress + cbIndex * objCBByteSize;
            
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = cbAddress;
            cbvDesc.SizeInBytes = objCBByteSize;

            CD3DX12_CPU_DESCRIPTOR_HANDLE viewHandle(mCbvHeap->GetCPUDescriptorHandleForHeapStart(), cbIndex, mCbvSrvUavDescriptorSize);

            md3dDevice->CreateConstantBufferView(&cbvDesc, viewHandle);
        }
    }
}

/**
 * 创建与常量缓冲区关联的着色器根参数与根签名，这个函数仅创建了签名/参数对象，没有做任何资源绑定；
 */
void BoxApp::BuildRootSignature()
{
    // Shader programs typically require resources as input (constant buffers,
    // textures, samplers).  The root signature defines the resources the shader
    // programs expect.  If we think of the shader programs as a function, and
    // the input resources as function parameters, then the root signature can be
    // thought of as defining the function signature.  

    // 第1步：创建shader中常量缓冲区对应的根参数
    
    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    {
        // Create a single descriptor table of CBVs.
        CD3DX12_DESCRIPTOR_RANGE cbvTable;
        // 指定两个描述符
        cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);
    }

    // 第2步：创建shader函数的根签名
    {
        // A root signature is an array of root parameters.
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
        ComPtr<ID3DBlob> serializedRootSig = nullptr;
        ComPtr<ID3DBlob> errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

        if (errorBlob != nullptr)
        {
            ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);

        ThrowIfFailed(md3dDevice->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(&mRootSignature)));

    }
}

/**
 * 编译着色器，并定义顶点输入布局。
 */
void BoxApp::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;
    
	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");
#if USING_SINGLE_VERTEX_BUFFER
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
#else
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
#endif
}

#if USING_SINGLE_VERTEX_BUFFER
/** 
 * 创建顶点缓冲区/索引缓冲区的资源对象（ID3D12Resource）与上传堆；
 */
void BoxApp::BuildBoxGeometry()
{
    std::array<Vertex, 13> vertices =
    {
        // first 8 for box
        Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) }),

        // then 5 for pyramid
        Vertex({ XMFLOAT3(-1.0f, 0, -1.0f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(-1.0f, 0, 1.0f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(+1.0f, 0, 1.0f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(+1.0f, 0, -1.0f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(0, 2, 0), XMFLOAT4(Colors::Red) })
    };

	std::array<std::uint16_t, 54> indices =
	{
        // first 36 for box

		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7,

        // then 18 for pymarid
        1, 4, 0,
        4, 3, 0,
        3, 4, 2,
        2, 4, 1,
        1, 3, 2,
        1, 0, 3,
	};

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mBoxGeo = std::make_unique<MeshGeometry>();
	mBoxGeo->Name = "boxGeo";

    // todo xiewneqi: 我们为什么需要VertexBufferCPU/IndexBufferCPU呢？好像没地方用到
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
	CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
	CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    // 创建顶点缓冲区ID3D12Resource资源堆与上传堆
	mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);

    // 创建索引缓冲区ID3D12Resource资源堆与上传堆
	mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

	mBoxGeo->VertexByteStride = sizeof(Vertex);
	mBoxGeo->VertexBufferByteSize = vbByteSize;
	mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mBoxGeo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = BoxIndexCount;
    boxSubmesh.StartIndexLocation = 0;
    boxSubmesh.BaseVertexLocation = 0;
	mBoxGeo->DrawArgs["box"] = boxSubmesh;

    SubmeshGeometry pyramidSubmesh;
    pyramidSubmesh.IndexCount = PyramidIndexCount;
    pyramidSubmesh.StartIndexLocation = boxSubmesh.IndexCount;
    pyramidSubmesh.BaseVertexLocation = BoxVertexCount;
    mBoxGeo->DrawArgs["pyramid"] = pyramidSubmesh;
}
#else
void BoxApp::BuildBoxGeometryMultiVertexBuffers()
{
    std::array<VertexPosData, 8> verticesPos =
    {
        VertexPosData({ XMFLOAT3(-1.0f, -1.0f, -1.0f) }),
        VertexPosData({ XMFLOAT3(-1.0f, +1.0f, -1.0f) }),
        VertexPosData({ XMFLOAT3(+1.0f, +1.0f, -1.0f) }),
        VertexPosData({ XMFLOAT3(+1.0f, -1.0f, -1.0f) }),
        VertexPosData({ XMFLOAT3(-1.0f, -1.0f, +1.0f) }),
        VertexPosData({ XMFLOAT3(-1.0f, +1.0f, +1.0f) }),
        VertexPosData({ XMFLOAT3(+1.0f, +1.0f, +1.0f) }),
        VertexPosData({ XMFLOAT3(+1.0f, -1.0f, +1.0f) })
    };
    mVertexBufferPosSize = (UINT)verticesPos.size() * sizeof(VertexPosData);
    mVertexBufferPosGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), verticesPos.data(), mVertexBufferPosSize, mVertexBufferPosUploader);


    std::array<VertexColorData, 8> verticesColor =
    {
        VertexColorData({ XMFLOAT4(Colors::White) }),
        VertexColorData({ XMFLOAT4(Colors::Black) }),
        VertexColorData({ XMFLOAT4(Colors::Red) }),
        VertexColorData({ XMFLOAT4(Colors::Green) }),
        VertexColorData({ XMFLOAT4(Colors::Blue) }),
        VertexColorData({ XMFLOAT4(Colors::Yellow) }),
        VertexColorData({ XMFLOAT4(Colors::Cyan) }),
        VertexColorData({ XMFLOAT4(Colors::Magenta) })
    };
    mVertexBufferColorSize = (UINT)verticesColor.size() * sizeof(VertexColorData);
    mVertexBufferColorGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), verticesColor.data(), mVertexBufferColorSize, mVertexBufferColorUploader);

    std::array<std::uint16_t, 36> indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };
    mIndexCount = (UINT)indices.size();
    mIndexBufferSize = mIndexCount * sizeof(std::uint16_t);
    mIndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), mIndexBufferSize, mIndexBufferUploader);
    
}
#endif

/**
 * 创建PSO对象。
 */
void BoxApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), 
		mvsByteCode->GetBufferSize() 
	};
    psoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), 
		mpsByteCode->GetBufferSize() 
	};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}