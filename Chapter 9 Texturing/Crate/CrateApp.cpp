//***************************************************************************************
// CrateApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class CrateApp : public D3DApp
{
public:
    CrateApp(HINSTANCE hInstance);
    CrateApp(const CrateApp& rhs) = delete;
    CrateApp& operator=(const CrateApp& rhs) = delete;
    ~CrateApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual void OnKeyDown(WPARAM wParam, WPARAM lParam) override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateSampler(const GameTimer& gt);

	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	void ScaleTexTransform(float newDelta);
	void ChangeSamplerFilter(D3D12_FILTER newFilter);
	void ChangeSamplerAddressMode(D3D12_TEXTURE_ADDRESS_MODE newAddressMode);
	void MarkSamplerDirty();
	void CreateSamplerDescriptor();

	void BuildDefaultRootSignature();
	void BuildFlareRootSignature();


private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mFlareRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSamplerDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mOpaquePSO = nullptr;
	ComPtr<ID3D12PipelineState> mWireFramePSO = nullptr;

	ComPtr<ID3D12PipelineState> mOpaqueFlarePSO = nullptr;
	ComPtr<ID3D12PipelineState> mWireFrameFlarePSO = nullptr;

	bool mOpaqueDraw = true;
 
	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

    PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.3f*XM_PI;
	float mPhi = 0.4f*XM_PI;
	float mRadius = 5.0f;

    POINT mLastMousePos;

	float mTexTransformScale = 1.0f;
	const float mTexTransformScaleDelta = 0.01f;

	D3D12_FILTER mCurrentSamplerFilter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	D3D12_TEXTURE_ADDRESS_MODE mCurrentSamplerAddressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	bool mSamplerDirty = false;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        CrateApp theApp(hInstance);
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

CrateApp::CrateApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

CrateApp::~CrateApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool CrateApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

 
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
	BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void CrateApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void CrateApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	UpdateSampler(gt);
}

void CrateApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.;
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));

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

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get(), mSamplerDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// box和flare的根签名不一样啦，所以要分开绘制
	auto passCB = mCurrFrameResource->PassCB->Resource();
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	// 绘制box
	{
		auto currentRenderItem = mOpaqueRitems[0];

		// set pso
		mCommandList->SetPipelineState(mOpaqueDraw ? mOpaquePSO.Get() : mWireFramePSO.Get());

		// root signature
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		// cbPerObject
		mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

		// gsamLinear
		mCommandList->SetGraphicsRootDescriptorTable(4, mSamplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		mCommandList->IASetVertexBuffers(0, 1, &currentRenderItem->Geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&currentRenderItem->Geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(currentRenderItem->PrimitiveType);

		// diffuse map
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(currentRenderItem->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + currentRenderItem->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + currentRenderItem->Mat->MatCBIndex * matCBByteSize;

		mCommandList->SetGraphicsRootDescriptorTable(0, tex);
		mCommandList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		mCommandList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		mCommandList->DrawIndexedInstanced(currentRenderItem->IndexCount, 1, currentRenderItem->StartIndexLocation, currentRenderItem->BaseVertexLocation, 0);
	}

	// 绘制flare box
	{
		auto currentRenderItem = mOpaqueRitems[1];

		// set pso
		mCommandList->SetPipelineState(mOpaqueDraw ? mOpaqueFlarePSO.Get() : mWireFrameFlarePSO.Get());

		// root signature
		mCommandList->SetGraphicsRootSignature(mFlareRootSignature.Get());

		// cbPerObject
		mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

		// gsamLinear
		mCommandList->SetGraphicsRootDescriptorTable(4, mSamplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		mCommandList->IASetVertexBuffers(0, 1, &currentRenderItem->Geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&currentRenderItem->Geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(currentRenderItem->PrimitiveType);

		// diffuse map
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(currentRenderItem->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + currentRenderItem->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + currentRenderItem->Mat->MatCBIndex * matCBByteSize;

		mCommandList->SetGraphicsRootDescriptorTable(0, tex);
		mCommandList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		mCommandList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		mCommandList->DrawIndexedInstanced(currentRenderItem->IndexCount, 1, currentRenderItem->StartIndexLocation, currentRenderItem->BaseVertexLocation, 0);
	}

	

    // DrawRenderItems(mCommandList.Get(), mOpaqueRitems);
	{
		/*
		UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
		UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
		auto objectCB = mCurrFrameResource->ObjectCB->Resource();
		auto matCB = mCurrFrameResource->MaterialCB->Resource();

		// For each render item...
		for(size_t i = 0; i < ritems.size(); ++i)
		{
			auto ri = ritems[i];

			cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
			cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
			cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

			CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

			cmdList->SetGraphicsRootDescriptorTable(0, tex);
			cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
			cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

			cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
		}
		*/
	}

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void CrateApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void CrateApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void CrateApp::OnMouseMove(WPARAM btnState, int x, int y)
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
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.05f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.05f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void CrateApp::OnKeyDown(WPARAM wParam, WPARAM lParam)
{	
	switch (wParam)
	{
		// 放大UV
	case '1': 
		ScaleTexTransform(this->mTexTransformScaleDelta);
		break;

		// 缩小UV
	case '2': 
		ScaleTexTransform(-this->mTexTransformScaleDelta);
		break;
		
		// POINT过滤
	case '3':
		ChangeSamplerFilter(D3D12_FILTER_MIN_MAG_MIP_POINT);
		break;
		
		// LINEAR过滤
	case '4':
		ChangeSamplerFilter(D3D12_FILTER_MIN_MAG_MIP_LINEAR);
		break;
		
		// 各向异性过滤
	case '5':
		ChangeSamplerFilter(D3D12_FILTER_ANISOTROPIC);
		break;
		
		// WRAP寻址
	case '6':
		ChangeSamplerAddressMode(D3D12_TEXTURE_ADDRESS_MODE_WRAP);
		break;
		
		// CLAMP寻址
	case '7':
		ChangeSamplerAddressMode(D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		break;
		
		// BORDER寻址
	case '8':
		ChangeSamplerAddressMode(D3D12_TEXTURE_ADDRESS_MODE_BORDER);
		break;
		
		// MIRROR寻址
	case '9':
		ChangeSamplerAddressMode(D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
		break;

	case VK_F1:
		mOpaqueDraw = true;
		break;

	case VK_F2:
		mOpaqueDraw = false;
		break;

	}
}

void CrateApp::ScaleTexTransform(float newScaleDelta)
{
	mTexTransformScale = max(mTexTransformScale + newScaleDelta, 0.1f);
	for (auto& e : mAllRitems)
	{
		XMMATRIX newScaleMatrix = XMMatrixScaling(mTexTransformScale, mTexTransformScale, 1);
		XMStoreFloat4x4(&e->TexTransform, newScaleMatrix);
		e->NumFramesDirty = gNumFrameResources;
	}
}

void CrateApp::ChangeSamplerFilter(D3D12_FILTER newFilter)
{
	if (newFilter != mCurrentSamplerFilter)
	{
		mCurrentSamplerFilter = newFilter;
		MarkSamplerDirty();
	}
}

void CrateApp::ChangeSamplerAddressMode(D3D12_TEXTURE_ADDRESS_MODE newAddressMode)
{
	if (newAddressMode != mCurrentSamplerAddressMode)
	{
		mCurrentSamplerAddressMode = newAddressMode;
		MarkSamplerDirty();
	}
}

void CrateApp::MarkSamplerDirty()
{
	mSamplerDirty = true;
}

void CrateApp::CreateSamplerDescriptor()
{
	XMFLOAT4 color = {1.0f, 1.0f, 0.0f, 1.0f};

	D3D12_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = mCurrentSamplerFilter;
	samplerDesc.AddressU = mCurrentSamplerAddressMode;
	samplerDesc.AddressV = mCurrentSamplerAddressMode;
	samplerDesc.AddressW = mCurrentSamplerAddressMode;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	memcpy(samplerDesc.BorderColor, & color, sizeof(samplerDesc.BorderColor));
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	md3dDevice->CreateSampler(&samplerDesc, mSamplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void CrateApp::OnKeyboardInput(const GameTimer& gt)
{
	
}
 
void CrateApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}



void CrateApp::AnimateMaterials(const GameTimer& gt)
{
	auto item = mOpaqueRitems[1];

	// 默认旋转是相对于UV的原点（左上角）进行的，我们需要让它相对于UV中心(0.5, 0.5)来旋转。
	
	// 先向左上方平移(0.5, 0.5)，然后旋转，再平移回原来的位置
	XMMATRIX m1 = XMMatrixMultiply(XMMatrixTranslation(-0.5f, -0.5f, 0), XMMatrixRotationZ(1.0f * gt.TotalTime()));
	XMMATRIX m2 = XMMatrixMultiply(m1, XMMatrixTranslation(0.5f, 0.5f, 0));
	XMStoreFloat4x4(&item->TexTransform, m2);

	item->NumFramesDirty = gNumFrameResources;
}

void CrateApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void CrateApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void CrateApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void CrateApp::UpdateSampler(const GameTimer& gt)
{
	if (mSamplerDirty)
	{
		CreateSamplerDescriptor();
		mSamplerDirty = false;
	}
}

void CrateApp::LoadTextures()
{
	// wood crate tex
	{
		auto woodCrateTex = std::make_unique<Texture>();
		woodCrateTex->Name = "woodCrateTex";
		woodCrateTex->Filename = L"../../Textures/mipmaps.dds";
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), woodCrateTex->Filename.c_str(),
			woodCrateTex->Resource, woodCrateTex->UploadHeap));

		mTextures[woodCrateTex->Name] = std::move(woodCrateTex);
	}

	// todo xiewneqi: 后面把wood crate tex 加回来，用键盘控制和mipmap的切换显示

	// flare and flare-alpha
	{
		auto flareTex = std::make_unique<Texture>();
		flareTex->Name = "flareTex";
		flareTex->Filename = L"../../Textures/flare.dds";
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), flareTex->Filename.c_str(), 
			flareTex->Resource, flareTex->UploadHeap));
		mTextures[flareTex->Name] = std::move(flareTex);

		auto flareAlphaTex = std::make_unique<Texture>();
		flareAlphaTex->Name = "flareAlphaTex";
		flareAlphaTex->Filename = L"../../Textures/flarealpha.dds";
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), flareAlphaTex->Filename.c_str(),
			flareAlphaTex->Resource, flareAlphaTex->UploadHeap));
		mTextures[flareAlphaTex->Name] = std::move(flareAlphaTex);
	}
}

void CrateApp::BuildRootSignature()
{
	BuildDefaultRootSignature();
	BuildFlareRootSignature();
}

void CrateApp::BuildDefaultRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE samplerRange;
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);
	slotRootParameter[4].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
		0, nullptr,
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
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void CrateApp::BuildFlareRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

	CD3DX12_DESCRIPTOR_RANGE samplerRange;
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);
	slotRootParameter[4].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
		0, nullptr,
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
		IID_PPV_ARGS(mFlareRootSignature.GetAddressOf())));
}

void CrateApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 3;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto woodCrateTex = mTextures["woodCrateTex"]->Resource;
 
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = woodCrateTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = woodCrateTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	md3dDevice->CreateShaderResourceView(woodCrateTex.Get(), &srvDesc, hDescriptor);

	// flare
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptorForFlare(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, mCbvSrvUavDescriptorSize);
	auto flareTexture = mTextures["flareTex"]->Resource;
	srvDesc.Format = flareTexture->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = flareTexture->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(flareTexture.Get(), &srvDesc, hDescriptorForFlare);

	// flarealpha
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptorForFlareAlpha(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 2, mCbvSrvUavDescriptorSize);
	auto flareAlphaTexture = mTextures["flareAlphaTex"]->Resource;
	srvDesc.Format = flareAlphaTexture->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = flareAlphaTexture->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(flareAlphaTexture.Get(), &srvDesc, hDescriptorForFlareAlpha);

	// 创建采样器描述符堆
	{
		// create heap
		D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
		samplerHeapDesc.NumDescriptors = 1;
		samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&mSamplerDescriptorHeap)));

		// create sampler
		CreateSamplerDescriptor();
	}
}

void CrateApp::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");
	
	mShaders["flareVS"] = d3dUtil::CompileShader(L"Shaders\\Flare.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["flarePS"] = d3dUtil::CompileShader(L"Shaders\\Flare.hlsl", nullptr, "PS", "ps_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };


}

void CrateApp::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
 
	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = 0;
	boxSubmesh.BaseVertexLocation = 0;

 
	std::vector<Vertex> vertices(box.Vertices.size());

	for(size_t i = 0; i < box.Vertices.size(); ++i)
	{
		vertices[i].Pos = box.Vertices[i].Position;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void CrateApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque box.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mOpaquePSO)));

	// pso for wireframe box
	D3D12_GRAPHICS_PIPELINE_STATE_DESC wireFramePsoDesc = opaquePsoDesc;
	wireFramePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&wireFramePsoDesc, IID_PPV_ARGS(&mWireFramePSO)));

	// pso for opaque flare box
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueFlarePsoDesc = opaquePsoDesc;
	opaqueFlarePsoDesc.pRootSignature = mFlareRootSignature.Get();
	opaqueFlarePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["flareVS"]->GetBufferPointer()),
		mShaders["flareVS"]->GetBufferSize()
	};
	opaqueFlarePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["flarePS"]->GetBufferPointer()),
		mShaders["flarePS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueFlarePsoDesc, IID_PPV_ARGS(&mOpaqueFlarePSO)));

	// pso for wireframe flare box
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaqueFlarePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mWireFrameFlarePSO)));

}

void CrateApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
    }
}

void CrateApp::BuildMaterials()
{
	// box
	auto woodCrate = std::make_unique<Material>();
	woodCrate->Name = "woodCrate";
	woodCrate->MatCBIndex = 0;
	woodCrate->DiffuseSrvHeapIndex = 0;
	woodCrate->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	woodCrate->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	woodCrate->Roughness = 0.2f;
	mMaterials["woodCrate"] = std::move(woodCrate);

	// flare box
	auto flareBox = std::make_unique<Material>();
	flareBox->Name = "flareBox";
	flareBox->MatCBIndex = 1;
	flareBox->DiffuseSrvHeapIndex = 1;
	flareBox->AlphaSrvHeapIndex = 2;
	flareBox->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	flareBox->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	flareBox->Roughness = 0.2f;
	mMaterials["flareBox"] = std::move(flareBox);
}

void CrateApp::BuildRenderItems()
{
	// box
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->ObjCBIndex = 0;
	boxRitem->Mat = mMaterials["woodCrate"].get();
	boxRitem->Geo = mGeometries["boxGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(-0.75f, 0, 0));
	mAllRitems.push_back(std::move(boxRitem));

	// flare box
	auto flareBox = std::make_unique<RenderItem>();
	flareBox->ObjCBIndex = 1;
	flareBox->Mat = mMaterials["flareBox"].get();
	flareBox->Geo = mGeometries["boxGeo"].get();
	flareBox->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	flareBox->IndexCount = flareBox->Geo->DrawArgs["box"].IndexCount;
	flareBox->StartIndexLocation = flareBox->Geo->DrawArgs["box"].StartIndexLocation;
	flareBox->BaseVertexLocation = flareBox->Geo->DrawArgs["box"].BaseVertexLocation;
	XMStoreFloat4x4(&flareBox->World, XMMatrixTranslation(0.75f, 0, 0));
	mAllRitems.push_back(std::move(flareBox));

	// All the render items are opaque.
	for(auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void CrateApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> CrateApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

