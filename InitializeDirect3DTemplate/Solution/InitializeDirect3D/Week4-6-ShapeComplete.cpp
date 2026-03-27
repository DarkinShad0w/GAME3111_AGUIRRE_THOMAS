#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	int NumFramesDirty = gNumFrameResources;

	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTestedTreeSprites,
	Count
};

class ShapesApp : public D3DApp
{
public:
	ShapesApp(HINSTANCE hInstance);
	ShapesApp(const ShapesApp& rhs) = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void LoadTextures();
	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildWaterGeometry();
	void BuildTreeSpritesGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	RenderItem* mWaterRitem = nullptr;

	PassConstants mMainPassCB;

	UINT mPassCbvOffset = 0;

	bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.2f * XM_PI;
	float mRadius = 15.0f;

	POINT mLastMousePos;
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
		ShapesApp theApp(hInstance);
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

ShapesApp::ShapesApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool ShapesApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildWaterGeometry();
	BuildTreeSpritesGeometry();
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

void ShapesApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void ShapesApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void ShapesApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

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

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	// Draw OPAQUE
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	// Draw TREES 
	mCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	// Draw TRANSPARENT 
	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

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

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;
}

void ShapesApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			e->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			mat->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
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

	// Directional Lights (3 lights)
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	// Point Lights - 4 red lights at each corner tower
	// Northwest Tower
	mMainPassCB.Lights[3].Position = { -30.0f, 5.0f, 30.0f };
	mMainPassCB.Lights[3].Strength = { 1.0f, 0.2f, 0.2f };
	mMainPassCB.Lights[3].FalloffStart = 5.0f;
	mMainPassCB.Lights[3].FalloffEnd = 25.0f;

	// Northeast Tower
	mMainPassCB.Lights[4].Position = { 30.0f, 5.0f, 30.0f };
	mMainPassCB.Lights[4].Strength = { 1.0f, 0.2f, 0.2f };
	mMainPassCB.Lights[4].FalloffStart = 5.0f;
	mMainPassCB.Lights[4].FalloffEnd = 25.0f;

	// Southwest Tower
	mMainPassCB.Lights[5].Position = { -30.0f, 5.0f, -30.0f };
	mMainPassCB.Lights[5].Strength = { 1.0f, 0.2f, 0.2f };
	mMainPassCB.Lights[5].FalloffStart = 5.0f;
	mMainPassCB.Lights[5].FalloffEnd = 25.0f;

	// Southeast Tower
	mMainPassCB.Lights[6].Position = { 30.0f, 5.0f, -30.0f };
	mMainPassCB.Lights[6].Strength = { 1.0f, 0.2f, 0.2f };
	mMainPassCB.Lights[6].FalloffStart = 5.0f;
	mMainPassCB.Lights[6].FalloffEnd = 25.0f;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::LoadTextures()
{
	auto stoneTex = std::make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->Filename = L"../../Textures/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), stoneTex->Filename.c_str(),
		stoneTex->Resource, stoneTex->UploadHeap));

	auto darkStoneTex = std::make_unique<Texture>();
	darkStoneTex->Name = "darkStoneTex";
	darkStoneTex->Filename = L"../../Textures/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), darkStoneTex->Filename.c_str(),
		darkStoneTex->Resource, darkStoneTex->UploadHeap));

	auto brickTex = std::make_unique<Texture>();
	brickTex->Name = "brickTex";
	brickTex->Filename = L"../../Textures/bricks.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), brickTex->Filename.c_str(),
		brickTex->Resource, brickTex->UploadHeap));

	auto roofTex = std::make_unique<Texture>();
	roofTex->Name = "roofTex";
	roofTex->Filename = L"../../Textures/tile.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), roofTex->Filename.c_str(),
		roofTex->Resource, roofTex->UploadHeap));

	auto goldTex = std::make_unique<Texture>();
	goldTex->Name = "goldTex";
	goldTex->Filename = L"../../Textures/bricks.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), goldTex->Filename.c_str(),
		goldTex->Resource, goldTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"../../Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"../../Textures/treeArray2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));


	mTextures[stoneTex->Name] = std::move(stoneTex);
	mTextures[darkStoneTex->Name] = std::move(darkStoneTex);
	mTextures[brickTex->Name] = std::move(brickTex);
	mTextures[roofTex->Name] = std::move(roofTex);
	mTextures[goldTex->Name] = std::move(goldTex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);
	mTextures["waterTex"] = std::move(waterTex);
}


void ShapesApp::BuildDescriptorHeaps()
{
	// Create the SRV heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = (UINT)mTextures.size();
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	// Fill out the heap with actual descriptors.
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (auto& texPair : mTextures)
	{
		auto texture = texPair.second->Resource;
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		auto desc = texture->GetDesc();

		// Check if this is the tree texture array
		if (texPair.first == "treeArrayTex")
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Format = desc.Format;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.MipLevels = -1;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Format = desc.Format;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = desc.MipLevels;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		md3dDevice->CreateShaderResourceView(texture.Get(), &srvDesc, hDescriptor);
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}
}

void ShapesApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // register t0

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0); // register b0 (ObjectCB)
	slotRootParameter[2].InitAsConstantBufferView(1); // register b1 (PassCB)
	slotRootParameter[3].InitAsConstantBufferView(2); // register b2 (MaterialCB)

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
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

void ShapesApp::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST",
		NULL, NULL
	};

	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_0");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}


void ShapesApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;

	// CASTLE FOUNDATION AND BASE
	GeometryGenerator::MeshData ground = geoGen.CreateGrid(80.0f, 80.0f, 60, 40);
	GeometryGenerator::MeshData keepFoundation = geoGen.CreateBox(20.0f, 2.0f, 15.0f, 0);
	GeometryGenerator::MeshData keepBody = geoGen.CreateBox(10.0f, 30.0f, 12.0f, 0);

	// OUTER WALLS
	GeometryGenerator::MeshData outerWallLong = geoGen.CreateBox(60.0f, 6.0f, 2.0f, 0);   // North/South walls
	GeometryGenerator::MeshData outerWallShort = geoGen.CreateBox(2.0f, 6.0f, 60.0f, 0);  // East/West walls

	// HEXAGONAL CORNER TOWERS
	GeometryGenerator::MeshData hexTower = geoGen.CreateHexagonalPrism(3.0f, 18.0f);

	// TOWER ROOFS
	GeometryGenerator::MeshData torusRoof = geoGen.CreateTorus(3.2f, 2.5f, 20, 20);

	// MAIN ROOF
	GeometryGenerator::MeshData keepPyramidRoof = geoGen.CreatePyramid(16.0f, 13.0f, 8.0f);

	// SIDE TOWER ROOFS
	GeometryGenerator::MeshData keepConeRoof = geoGen.CreateCone(2.5f, 6.0f, 16, 8);

	// SPIRE
	GeometryGenerator::MeshData diamondSpire = geoGen.CreateDiamond(4.0f, 3.0f);

	// TRIANGLE PRISM
	GeometryGenerator::MeshData arrowSlit = geoGen.CreateTriangularPrism(0.8f, 0.3f, 5.0f);

	// WEDGE
	GeometryGenerator::MeshData gableWedge = geoGen.CreateWedge(4.0f, 3.0f, 0.5f);

	// GATE COLUMNS
	GeometryGenerator::MeshData gateColumn = geoGen.CreateCylinder(1.0f, 1.0f, 8.0f, 12, 4);

	// GATE
	GeometryGenerator::MeshData gatehouse = geoGen.CreateBox(12.0f, 8.0f, 4.0f, 0);

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT groundVertexOffset = 0;
	UINT keepFoundationVertexOffset = (UINT)ground.Vertices.size();
	UINT keepBodyVertexOffset = keepFoundationVertexOffset + (UINT)keepFoundation.Vertices.size();
	UINT outerWallLongVertexOffset = keepBodyVertexOffset + (UINT)keepBody.Vertices.size();
	UINT outerWallShortVertexOffset = outerWallLongVertexOffset + (UINT)outerWallLong.Vertices.size();
	UINT hexTowerVertexOffset = outerWallShortVertexOffset + (UINT)outerWallShort.Vertices.size();
	UINT torusRoofVertexOffset = hexTowerVertexOffset + (UINT)hexTower.Vertices.size();
	UINT keepPyramidRoofVertexOffset = torusRoofVertexOffset + (UINT)torusRoof.Vertices.size();
	UINT keepConeRoofVertexOffset = keepPyramidRoofVertexOffset + (UINT)keepPyramidRoof.Vertices.size();
	UINT diamondSpireVertexOffset = keepConeRoofVertexOffset + (UINT)keepConeRoof.Vertices.size();
	UINT arrowSlitVertexOffset = diamondSpireVertexOffset + (UINT)diamondSpire.Vertices.size();
	UINT gableWedgeVertexOffset = arrowSlitVertexOffset + (UINT)arrowSlit.Vertices.size();
	UINT gateColumnVertexOffset = gableWedgeVertexOffset + (UINT)gableWedge.Vertices.size();
	UINT gatehouseVertexOffset = gateColumnVertexOffset + (UINT)gateColumn.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT groundIndexOffset = 0;
	UINT keepFoundationIndexOffset = (UINT)ground.Indices32.size();
	UINT keepBodyIndexOffset = keepFoundationIndexOffset + (UINT)keepFoundation.Indices32.size();
	UINT outerWallLongIndexOffset = keepBodyIndexOffset + (UINT)keepBody.Indices32.size();
	UINT outerWallShortIndexOffset = outerWallLongIndexOffset + (UINT)outerWallLong.Indices32.size();
	UINT hexTowerIndexOffset = outerWallShortIndexOffset + (UINT)outerWallShort.Indices32.size();
	UINT torusRoofIndexOffset = hexTowerIndexOffset + (UINT)hexTower.Indices32.size();
	UINT keepPyramidRoofIndexOffset = torusRoofIndexOffset + (UINT)torusRoof.Indices32.size();
	UINT keepConeRoofIndexOffset = keepPyramidRoofIndexOffset + (UINT)keepPyramidRoof.Indices32.size();
	UINT diamondSpireIndexOffset = keepConeRoofIndexOffset + (UINT)keepConeRoof.Indices32.size();
	UINT arrowSlitIndexOffset = diamondSpireIndexOffset + (UINT)diamondSpire.Indices32.size();
	UINT gableWedgeIndexOffset = arrowSlitIndexOffset + (UINT)arrowSlit.Indices32.size();
	UINT gateColumnIndexOffset = gableWedgeIndexOffset + (UINT)gableWedge.Indices32.size();
	UINT gatehouseIndexOffset = gateColumnIndexOffset + (UINT)gateColumn.Indices32.size();

	// Define the SubmeshGeometry that cover different
	// regions of the vertex/index buffers.
	SubmeshGeometry groundSubmesh;
	groundSubmesh.IndexCount = (UINT)ground.Indices32.size();
	groundSubmesh.StartIndexLocation = groundIndexOffset;
	groundSubmesh.BaseVertexLocation = groundVertexOffset;

	SubmeshGeometry keepFoundationSubmesh;
	keepFoundationSubmesh.IndexCount = (UINT)keepFoundation.Indices32.size();
	keepFoundationSubmesh.StartIndexLocation = keepFoundationIndexOffset;
	keepFoundationSubmesh.BaseVertexLocation = keepFoundationVertexOffset;

	SubmeshGeometry keepBodySubmesh;
	keepBodySubmesh.IndexCount = (UINT)keepBody.Indices32.size();
	keepBodySubmesh.StartIndexLocation = keepBodyIndexOffset;
	keepBodySubmesh.BaseVertexLocation = keepBodyVertexOffset;

	SubmeshGeometry outerWallLongSubmesh;
	outerWallLongSubmesh.IndexCount = (UINT)outerWallLong.Indices32.size();
	outerWallLongSubmesh.StartIndexLocation = outerWallLongIndexOffset;
	outerWallLongSubmesh.BaseVertexLocation = outerWallLongVertexOffset;

	SubmeshGeometry outerWallShortSubmesh;
	outerWallShortSubmesh.IndexCount = (UINT)outerWallShort.Indices32.size();
	outerWallShortSubmesh.StartIndexLocation = outerWallShortIndexOffset;
	outerWallShortSubmesh.BaseVertexLocation = outerWallShortVertexOffset;

	SubmeshGeometry hexTowerSubmesh;
	hexTowerSubmesh.IndexCount = (UINT)hexTower.Indices32.size();
	hexTowerSubmesh.StartIndexLocation = hexTowerIndexOffset;
	hexTowerSubmesh.BaseVertexLocation = hexTowerVertexOffset;

	SubmeshGeometry torusRoofSubmesh;
	torusRoofSubmesh.IndexCount = (UINT)torusRoof.Indices32.size();
	torusRoofSubmesh.StartIndexLocation = torusRoofIndexOffset;
	torusRoofSubmesh.BaseVertexLocation = torusRoofVertexOffset;

	SubmeshGeometry keepPyramidRoofSubmesh;
	keepPyramidRoofSubmesh.IndexCount = (UINT)keepPyramidRoof.Indices32.size();
	keepPyramidRoofSubmesh.StartIndexLocation = keepPyramidRoofIndexOffset;
	keepPyramidRoofSubmesh.BaseVertexLocation = keepPyramidRoofVertexOffset;

	SubmeshGeometry keepConeRoofSubmesh;
	keepConeRoofSubmesh.IndexCount = (UINT)keepConeRoof.Indices32.size();
	keepConeRoofSubmesh.StartIndexLocation = keepConeRoofIndexOffset;
	keepConeRoofSubmesh.BaseVertexLocation = keepConeRoofVertexOffset;

	SubmeshGeometry diamondSpireSubmesh;
	diamondSpireSubmesh.IndexCount = (UINT)diamondSpire.Indices32.size();
	diamondSpireSubmesh.StartIndexLocation = diamondSpireIndexOffset;
	diamondSpireSubmesh.BaseVertexLocation = diamondSpireVertexOffset;

	SubmeshGeometry arrowSlitSubmesh;
	arrowSlitSubmesh.IndexCount = (UINT)arrowSlit.Indices32.size();
	arrowSlitSubmesh.StartIndexLocation = arrowSlitIndexOffset;
	arrowSlitSubmesh.BaseVertexLocation = arrowSlitVertexOffset;

	SubmeshGeometry gableWedgeSubmesh;
	gableWedgeSubmesh.IndexCount = (UINT)gableWedge.Indices32.size();
	gableWedgeSubmesh.StartIndexLocation = gableWedgeIndexOffset;
	gableWedgeSubmesh.BaseVertexLocation = gableWedgeVertexOffset;

	SubmeshGeometry gateColumnSubmesh;
	gateColumnSubmesh.IndexCount = (UINT)gateColumn.Indices32.size();
	gateColumnSubmesh.StartIndexLocation = gateColumnIndexOffset;
	gateColumnSubmesh.BaseVertexLocation = gateColumnVertexOffset;

	SubmeshGeometry gatehouseSubmesh;
	gatehouseSubmesh.IndexCount = (UINT)gatehouse.Indices32.size();
	gatehouseSubmesh.StartIndexLocation = gatehouseIndexOffset;
	gatehouseSubmesh.BaseVertexLocation = gatehouseVertexOffset;

	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.

	auto totalVertexCount =
		ground.Vertices.size() +
		keepFoundation.Vertices.size() +
		keepBody.Vertices.size() +
		outerWallLong.Vertices.size() +
		outerWallShort.Vertices.size() +
		hexTower.Vertices.size() +
		torusRoof.Vertices.size() +
		keepPyramidRoof.Vertices.size() +
		keepConeRoof.Vertices.size() +
		diamondSpire.Vertices.size() +
		arrowSlit.Vertices.size() +
		gableWedge.Vertices.size() +
		gateColumn.Vertices.size() +
		gatehouse.Vertices.size();


	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;

	// Ground - dark green
	for (size_t i = 0; i < ground.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = ground.Vertices[i].Position;
		vertices[k].Normal = ground.Vertices[i].Normal;
		vertices[k].TexC = ground.Vertices[i].TexC;
	}

	// Keep Foundation - dark gray
	for (size_t i = 0; i < keepFoundation.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = keepFoundation.Vertices[i].Position;
		vertices[k].Normal = keepFoundation.Vertices[i].Normal;
		vertices[k].TexC = keepFoundation.Vertices[i].TexC;
	}

	// Keep Body - light gray
	for (size_t i = 0; i < keepBody.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = keepBody.Vertices[i].Position;
		vertices[k].Normal = keepBody.Vertices[i].Normal;
		vertices[k].TexC = keepBody.Vertices[i].TexC;
	}

	// Outer Walls Long - stone gray
	for (size_t i = 0; i < outerWallLong.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = outerWallLong.Vertices[i].Position;
		vertices[k].Normal = outerWallLong.Vertices[i].Normal;
		vertices[k].TexC = outerWallLong.Vertices[i].TexC;
	}

	// Outer Walls Short - stone gray
	for (size_t i = 0; i < outerWallShort.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = outerWallShort.Vertices[i].Position;
		vertices[k].Normal = outerWallShort.Vertices[i].Normal;
		vertices[k].TexC = outerWallShort.Vertices[i].TexC;
	}

	// Hexagonal Towers - medium gray
	for (size_t i = 0; i < hexTower.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = hexTower.Vertices[i].Position;
		vertices[k].Normal = hexTower.Vertices[i].Normal;
		vertices[k].TexC = hexTower.Vertices[i].TexC;
	}

	// Torus Roofs - dark brown
	for (size_t i = 0; i < torusRoof.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = torusRoof.Vertices[i].Position;
		vertices[k].Normal = torusRoof.Vertices[i].Normal;
		vertices[k].TexC = torusRoof.Vertices[i].TexC;
	}

	// Keep Pyramid Roof - dark red
	for (size_t i = 0; i < keepPyramidRoof.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = keepPyramidRoof.Vertices[i].Position;
		vertices[k].Normal = keepPyramidRoof.Vertices[i].Normal;
		vertices[k].TexC = keepPyramidRoof.Vertices[i].TexC;
	}

	// Keep Cone Roofs - dark brown
	for (size_t i = 0; i < keepConeRoof.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = keepConeRoof.Vertices[i].Position;
		vertices[k].Normal = keepConeRoof.Vertices[i].Normal;
		vertices[k].TexC = keepConeRoof.Vertices[i].TexC;
	}

	// Diamond Spire - gold
	for (size_t i = 0; i < diamondSpire.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = diamondSpire.Vertices[i].Position;
		vertices[k].Normal = diamondSpire.Vertices[i].Normal;
		vertices[k].TexC = diamondSpire.Vertices[i].TexC;
	}

	// Arrow Slits - black
	for (size_t i = 0; i < arrowSlit.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = arrowSlit.Vertices[i].Position;
		vertices[k].Normal = arrowSlit.Vertices[i].Normal;
		vertices[k].TexC = arrowSlit.Vertices[i].TexC;
	}

	// Gable Wedges - medium gray
	for (size_t i = 0; i < gableWedge.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = gableWedge.Vertices[i].Position;
		vertices[k].Normal = gableWedge.Vertices[i].Normal;
		vertices[k].TexC = gableWedge.Vertices[i].TexC;
	}

	// Gate Columns - Dark Gray
	for (size_t i = 0; i < gateColumn.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = gateColumn.Vertices[i].Position;
		vertices[k].Normal = gateColumn.Vertices[i].Normal;
		vertices[k].TexC = gateColumn.Vertices[i].TexC;
	}

	// Gatehouse - Medium Gray
	for (size_t i = 0; i < gatehouse.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = gatehouse.Vertices[i].Position;
		vertices[k].Normal = gatehouse.Vertices[i].Normal;
		vertices[k].TexC = gatehouse.Vertices[i].TexC;
	}


	// Create indices
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(ground.GetIndices16()), std::end(ground.GetIndices16()));
	indices.insert(indices.end(), std::begin(keepFoundation.GetIndices16()), std::end(keepFoundation.GetIndices16()));
	indices.insert(indices.end(), std::begin(keepBody.GetIndices16()), std::end(keepBody.GetIndices16()));
	indices.insert(indices.end(), std::begin(outerWallLong.GetIndices16()), std::end(outerWallLong.GetIndices16()));
	indices.insert(indices.end(), std::begin(outerWallShort.GetIndices16()), std::end(outerWallShort.GetIndices16()));
	indices.insert(indices.end(), std::begin(hexTower.GetIndices16()), std::end(hexTower.GetIndices16()));
	indices.insert(indices.end(), std::begin(torusRoof.GetIndices16()), std::end(torusRoof.GetIndices16()));
	indices.insert(indices.end(), std::begin(keepPyramidRoof.GetIndices16()), std::end(keepPyramidRoof.GetIndices16()));
	indices.insert(indices.end(), std::begin(keepConeRoof.GetIndices16()), std::end(keepConeRoof.GetIndices16()));
	indices.insert(indices.end(), std::begin(diamondSpire.GetIndices16()), std::end(diamondSpire.GetIndices16()));
	indices.insert(indices.end(), std::begin(arrowSlit.GetIndices16()), std::end(arrowSlit.GetIndices16()));
	indices.insert(indices.end(), std::begin(gableWedge.GetIndices16()), std::end(gableWedge.GetIndices16()));
	indices.insert(indices.end(), std::begin(gateColumn.GetIndices16()), std::end(gateColumn.GetIndices16()));
	indices.insert(indices.end(), std::begin(gatehouse.GetIndices16()), std::end(gatehouse.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "castleGeo";

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

	// Store submesh data
	geo->DrawArgs["ground"] = groundSubmesh;
	geo->DrawArgs["keepFoundation"] = keepFoundationSubmesh;
	geo->DrawArgs["keepBody"] = keepBodySubmesh;
	geo->DrawArgs["outerWallLong"] = outerWallLongSubmesh;
	geo->DrawArgs["outerWallShort"] = outerWallShortSubmesh;
	geo->DrawArgs["hexTower"] = hexTowerSubmesh;
	geo->DrawArgs["torusRoof"] = torusRoofSubmesh;
	geo->DrawArgs["keepPyramidRoof"] = keepPyramidRoofSubmesh;
	geo->DrawArgs["keepConeRoof"] = keepConeRoofSubmesh;
	geo->DrawArgs["diamondSpire"] = diamondSpireSubmesh;
	geo->DrawArgs["arrowSlit"] = arrowSlitSubmesh;
	geo->DrawArgs["gableWedge"] = gableWedgeSubmesh;
	geo->DrawArgs["gateColumn"] = gateColumnSubmesh;
	geo->DrawArgs["gatehouse"] = gatehouseSubmesh;

	mGeometries[geo->Name] = std::move(geo);

}

void ShapesApp::BuildWaterGeometry()
{
	GeometryGenerator geoGen;

	GeometryGenerator::MeshData waterPlane = geoGen.CreateGrid(200.0f, 200.0f, 100, 100);

	std::vector<Vertex> vertices(waterPlane.Vertices.size());
	for (size_t i = 0; i < waterPlane.Vertices.size(); ++i)
	{
		vertices[i].Pos = waterPlane.Vertices[i].Position;
		vertices[i].Pos.y = -0.7f;
		vertices[i].Normal = waterPlane.Vertices[i].Normal;
		vertices[i].TexC = waterPlane.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = waterPlane.GetIndices16();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

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

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["water"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void ShapesApp::BuildTreeSpritesGeometry()
{
	struct TreeVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	// Create 30 trees randomly
	const int treeCount = 30;
	std::vector<TreeVertex> vertices(treeCount);

	for (int i = 0; i < treeCount; ++i)
	{
		float angle = MathHelper::RandF(0.0f, XM_2PI);
		float radius = MathHelper::RandF(18.0f, 45.0f);

		float x = cosf(angle) * radius;
		float z = sinf(angle) * radius;

		float groundY = -0.5f;
		float treeY = groundY + 5.0f;

		vertices[i].Pos = XMFLOAT3(x, treeY, z);

		float width = MathHelper::RandF(5.0f, 10.0f);
		float height = MathHelper::RandF(8.0f, 14.0f);
		vertices[i].Size = XMFLOAT2(width, height);
	}

	std::vector<std::uint16_t> indices;
	for (int i = 0; i < treeCount; ++i)
		indices.push_back(i);

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(TreeVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;

	mGeometries["treeGeo"] = std::move(geo);
}

void ShapesApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

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
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC treePsoDesc = opaquePsoDesc;

	treePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
		mShaders["treeSpriteVS"]->GetBufferSize()
	};

	treePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
		mShaders["treeSpriteGS"]->GetBufferSize()
	};

	treePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
		mShaders["treeSpritePS"]->GetBufferSize()
	};

	treePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };

	treePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"])));
}

void ShapesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
}

void ShapesApp::BuildMaterials()
{
	int heapIndex = 0;

	// Ground material - tile texture
	auto groundMat = std::make_unique<Material>();
	groundMat->Name = "groundMat";
	groundMat->MatCBIndex = heapIndex++;
	groundMat->DiffuseSrvHeapIndex = 3; // tileTex
	groundMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	groundMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	groundMat->Roughness = 0.5f;

	// Stone material
	auto stoneMat = std::make_unique<Material>();
	stoneMat->Name = "stoneMat";
	stoneMat->MatCBIndex = heapIndex++;
	stoneMat->DiffuseSrvHeapIndex = 0; // stoneTex
	stoneMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stoneMat->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	stoneMat->Roughness = 0.3f;

	// Dark stone material
	auto darkStoneMat = std::make_unique<Material>();
	darkStoneMat->Name = "darkStoneMat";
	darkStoneMat->MatCBIndex = heapIndex++;
	darkStoneMat->DiffuseSrvHeapIndex = 0; // stoneTex
	darkStoneMat->DiffuseAlbedo = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	darkStoneMat->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	darkStoneMat->Roughness = 0.4f;

	// Brick material
	auto brickMat = std::make_unique<Material>();
	brickMat->Name = "brickMat";
	brickMat->MatCBIndex = heapIndex++;
	brickMat->DiffuseSrvHeapIndex = 2; // brickTex
	brickMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	brickMat->FresnelR0 = XMFLOAT3(0.03f, 0.03f, 0.03f);
	brickMat->Roughness = 0.2f;

	// Roof material
	auto roofMat = std::make_unique<Material>();
	roofMat->Name = "roofMat";
	roofMat->MatCBIndex = heapIndex++;
	roofMat->DiffuseSrvHeapIndex = 3; // roofTex
	roofMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	roofMat->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	roofMat->Roughness = 0.6f;

	// Gold material
	auto goldMat = std::make_unique<Material>();
	goldMat->Name = "goldMat";
	goldMat->MatCBIndex = heapIndex++;
	goldMat->DiffuseSrvHeapIndex = 4; // goldTex
	goldMat->DiffuseAlbedo = XMFLOAT4(1.0f, 0.85f, 0.0f, 1.0f);
	goldMat->FresnelR0 = XMFLOAT3(0.8f, 0.7f, 0.2f);
	goldMat->Roughness = 0.1f;

	// tree
	auto treeMat = std::make_unique<Material>();
	treeMat->Name = "treeMat";
	treeMat->MatCBIndex = heapIndex++;
	treeMat->DiffuseSrvHeapIndex = 5; 
	treeMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeMat->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeMat->Roughness = 0.2f;

	// Water material with transparency
	auto waterMat = std::make_unique<Material>();
	waterMat->Name = "waterMat";
	waterMat->MatCBIndex = heapIndex++; 
	waterMat->DiffuseSrvHeapIndex = 6;  
	waterMat->DiffuseAlbedo = XMFLOAT4(0.2f, 0.4f, 0.8f, 0.6f); 
	waterMat->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	waterMat->Roughness = 0.1f;


	mMaterials["groundMat"] = std::move(groundMat);
	mMaterials["stoneMat"] = std::move(stoneMat);
	mMaterials["darkStoneMat"] = std::move(darkStoneMat);
	mMaterials["brickMat"] = std::move(brickMat);
	mMaterials["roofMat"] = std::move(roofMat);
	mMaterials["goldMat"] = std::move(goldMat);
	mMaterials["treeMat"] = std::move(treeMat);
	mMaterials["waterMat"] = std::move(waterMat);
	
}

void ShapesApp::BuildRenderItems()
{
	// Clear existing render items
	mAllRitems.clear();
	for (int i = 0; i < (int)RenderLayer::Count; ++i)
		mRitemLayer[i].clear();

	UINT objCBIndex = 0;

	

	// WATER
	auto waterRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&waterRitem->World, XMMatrixTranslation(0.0f, -0.7f, 0.0f));
	XMStoreFloat4x4(&waterRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	waterRitem->ObjCBIndex = objCBIndex++;
	waterRitem->Mat = mMaterials["waterMat"].get();
	waterRitem->Geo = mGeometries["waterGeo"].get();
	waterRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	waterRitem->IndexCount = waterRitem->Geo->DrawArgs["water"].IndexCount;
	waterRitem->StartIndexLocation = waterRitem->Geo->DrawArgs["water"].StartIndexLocation;
	waterRitem->BaseVertexLocation = waterRitem->Geo->DrawArgs["water"].BaseVertexLocation;

	mWaterRitem = waterRitem.get();
	mRitemLayer[(int)RenderLayer::Transparent].push_back(waterRitem.get());
	mAllRitems.push_back(std::move(waterRitem));

	// TREEE
	auto treeRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&treeRitem->World, XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	XMStoreFloat4x4(&treeRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	treeRitem->ObjCBIndex = objCBIndex++;
	treeRitem->Mat = mMaterials["treeMat"].get();
	treeRitem->Geo = mGeometries["treeGeo"].get();
	treeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeRitem->IndexCount = treeRitem->Geo->DrawArgs["points"].IndexCount;
	treeRitem->StartIndexLocation = treeRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeRitem->BaseVertexLocation = treeRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeRitem.get());
	mAllRitems.push_back(std::move(treeRitem));

	// GROUND 
	auto groundRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&groundRitem->World, XMMatrixTranslation(0.0f, -0.5f, 0.0f));
	XMStoreFloat4x4(&groundRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	groundRitem->ObjCBIndex = objCBIndex++;
	groundRitem->Mat = mMaterials["groundMat"].get();
	groundRitem->Geo = mGeometries["castleGeo"].get();
	groundRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	groundRitem->IndexCount = groundRitem->Geo->DrawArgs["ground"].IndexCount;
	groundRitem->StartIndexLocation = groundRitem->Geo->DrawArgs["ground"].StartIndexLocation;
	groundRitem->BaseVertexLocation = groundRitem->Geo->DrawArgs["ground"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(groundRitem.get());
	mAllRitems.push_back(std::move(groundRitem));

	// FOUNDATION
	auto keepFoundationRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&keepFoundationRitem->World, XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	XMStoreFloat4x4(&keepFoundationRitem->TexTransform, XMMatrixScaling(2.0f, 1.0f, 1.5f));
	keepFoundationRitem->ObjCBIndex = objCBIndex++;
	keepFoundationRitem->Mat = mMaterials["darkStoneMat"].get();
	keepFoundationRitem->Geo = mGeometries["castleGeo"].get();
	keepFoundationRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFoundationRitem->IndexCount = keepFoundationRitem->Geo->DrawArgs["keepFoundation"].IndexCount;
	keepFoundationRitem->StartIndexLocation = keepFoundationRitem->Geo->DrawArgs["keepFoundation"].StartIndexLocation;
	keepFoundationRitem->BaseVertexLocation = keepFoundationRitem->Geo->DrawArgs["keepFoundation"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepFoundationRitem.get());
	mAllRitems.push_back(std::move(keepFoundationRitem));

	// BODY
	auto keepBodyRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&keepBodyRitem->World, XMMatrixTranslation(0.0f, 6.0f, 0.0f));
	XMStoreFloat4x4(&keepBodyRitem->TexTransform, XMMatrixScaling(2.0f, 3.0f, 2.0f));
	keepBodyRitem->ObjCBIndex = objCBIndex++;
	keepBodyRitem->Mat = mMaterials["brickMat"].get();
	keepBodyRitem->Geo = mGeometries["castleGeo"].get();
	keepBodyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBodyRitem->IndexCount = keepBodyRitem->Geo->DrawArgs["keepBody"].IndexCount;
	keepBodyRitem->StartIndexLocation = keepBodyRitem->Geo->DrawArgs["keepBody"].StartIndexLocation;
	keepBodyRitem->BaseVertexLocation = keepBodyRitem->Geo->DrawArgs["keepBody"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepBodyRitem.get());
	mAllRitems.push_back(std::move(keepBodyRitem));

	// OUTER WALLS
	// North Wall (facing +Z)
	auto northWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&northWallRitem->World, XMMatrixTranslation(0.0f, 3.0f, 30.0f));
	XMStoreFloat4x4(&northWallRitem->TexTransform, XMMatrixScaling(6.0f, 1.0f, 1.0f));
	northWallRitem->ObjCBIndex = objCBIndex++;
	northWallRitem->Mat = mMaterials["stoneMat"].get();
	northWallRitem->Geo = mGeometries["castleGeo"].get();
	northWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	northWallRitem->IndexCount = northWallRitem->Geo->DrawArgs["outerWallLong"].IndexCount;
	northWallRitem->StartIndexLocation = northWallRitem->Geo->DrawArgs["outerWallLong"].StartIndexLocation;
	northWallRitem->BaseVertexLocation = northWallRitem->Geo->DrawArgs["outerWallLong"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(northWallRitem.get());
	mAllRitems.push_back(std::move(northWallRitem));

	// South Wall (facing -Z)
	auto southWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&southWallRitem->World, XMMatrixTranslation(0.0f, 3.0f, -30.0f));
	XMStoreFloat4x4(&southWallRitem->TexTransform, XMMatrixScaling(6.0f, 1.0f, 1.0f));
	southWallRitem->ObjCBIndex = objCBIndex++;
	southWallRitem->Mat = mMaterials["stoneMat"].get();
	southWallRitem->Geo = mGeometries["castleGeo"].get();
	southWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	southWallRitem->IndexCount = southWallRitem->Geo->DrawArgs["outerWallLong"].IndexCount;
	southWallRitem->StartIndexLocation = southWallRitem->Geo->DrawArgs["outerWallLong"].StartIndexLocation;
	southWallRitem->BaseVertexLocation = southWallRitem->Geo->DrawArgs["outerWallLong"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(southWallRitem.get());
	mAllRitems.push_back(std::move(southWallRitem));

	// East Wall (facing +X)
	auto eastWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&eastWallRitem->World, XMMatrixTranslation(30.0f, 3.0f, 0.0f));
	XMStoreFloat4x4(&eastWallRitem->TexTransform, XMMatrixScaling(6.0f, 1.0f, 1.0f));
	eastWallRitem->ObjCBIndex = objCBIndex++;
	eastWallRitem->Mat = mMaterials["stoneMat"].get();
	eastWallRitem->Geo = mGeometries["castleGeo"].get();
	eastWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	eastWallRitem->IndexCount = eastWallRitem->Geo->DrawArgs["outerWallShort"].IndexCount;
	eastWallRitem->StartIndexLocation = eastWallRitem->Geo->DrawArgs["outerWallShort"].StartIndexLocation;
	eastWallRitem->BaseVertexLocation = eastWallRitem->Geo->DrawArgs["outerWallShort"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(eastWallRitem.get());
	mAllRitems.push_back(std::move(eastWallRitem));

	// West Wall (facing -X)
	auto westWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&westWallRitem->World, XMMatrixTranslation(-30.0f, 3.0f, 0.0f));
	XMStoreFloat4x4(&westWallRitem->TexTransform, XMMatrixScaling(6.0f, 1.0f, 1.0f));
	westWallRitem->ObjCBIndex = objCBIndex++;
	westWallRitem->Mat = mMaterials["stoneMat"].get();
	westWallRitem->Geo = mGeometries["castleGeo"].get();
	westWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	westWallRitem->IndexCount = westWallRitem->Geo->DrawArgs["outerWallShort"].IndexCount;
	westWallRitem->StartIndexLocation = westWallRitem->Geo->DrawArgs["outerWallShort"].StartIndexLocation;
	westWallRitem->BaseVertexLocation = westWallRitem->Geo->DrawArgs["outerWallShort"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(westWallRitem.get());
	mAllRitems.push_back(std::move(westWallRitem));

	// HEXAGONAL CORNER TOWERS
	// Northwest Tower (-X, +Z)
	auto nwTowerRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&nwTowerRitem->World, XMMatrixTranslation(-30.0f, 1.0f, 30.0f));
	XMStoreFloat4x4(&nwTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	nwTowerRitem->ObjCBIndex = objCBIndex++;
	nwTowerRitem->Mat = mMaterials["brickMat"].get();
	nwTowerRitem->Geo = mGeometries["castleGeo"].get();
	nwTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	nwTowerRitem->IndexCount = nwTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	nwTowerRitem->StartIndexLocation = nwTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	nwTowerRitem->BaseVertexLocation = nwTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(nwTowerRitem.get());
	mAllRitems.push_back(std::move(nwTowerRitem));

	// Northeast Tower (+X, +Z)
	auto neTowerRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&neTowerRitem->World, XMMatrixTranslation(30.0f, 1.0f, 30.0f));
	XMStoreFloat4x4(&neTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	neTowerRitem->ObjCBIndex = objCBIndex++;
	neTowerRitem->Mat = mMaterials["brickMat"].get();
	neTowerRitem->Geo = mGeometries["castleGeo"].get();
	neTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	neTowerRitem->IndexCount = neTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	neTowerRitem->StartIndexLocation = neTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	neTowerRitem->BaseVertexLocation = neTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(neTowerRitem.get());
	mAllRitems.push_back(std::move(neTowerRitem));

	// Southwest Tower (-X, -Z)
	auto swTowerRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&swTowerRitem->World, XMMatrixTranslation(-30.0f, 1.0f, -30.0f));
	XMStoreFloat4x4(&swTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	swTowerRitem->ObjCBIndex = objCBIndex++;
	swTowerRitem->Mat = mMaterials["brickMat"].get();
	swTowerRitem->Geo = mGeometries["castleGeo"].get();
	swTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	swTowerRitem->IndexCount = swTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	swTowerRitem->StartIndexLocation = swTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	swTowerRitem->BaseVertexLocation = swTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(swTowerRitem.get());
	mAllRitems.push_back(std::move(swTowerRitem));

	// Southeast Tower (+X, -Z)
	auto seTowerRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&seTowerRitem->World, XMMatrixTranslation(30.0f, 1.0f, -30.0f));
	XMStoreFloat4x4(&seTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	seTowerRitem->ObjCBIndex = objCBIndex++;
	seTowerRitem->Mat = mMaterials["brickMat"].get();
	seTowerRitem->Geo = mGeometries["castleGeo"].get();
	seTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	seTowerRitem->IndexCount = seTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	seTowerRitem->StartIndexLocation = seTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	seTowerRitem->BaseVertexLocation = seTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(seTowerRitem.get());
	mAllRitems.push_back(std::move(seTowerRitem));

	// TORUS ROOFS ON TOWERS
	// Northwest Tower Roof
	auto nwTowerRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX nwRoofTransform = XMMatrixScaling(0.8f, 0.4f, 1.0f) * XMMatrixTranslation(-30.0f, 11.0f, 30.0f);
	XMStoreFloat4x4(&nwTowerRoofRitem->World, nwRoofTransform);
	XMStoreFloat4x4(&nwTowerRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	nwTowerRoofRitem->ObjCBIndex = objCBIndex++;
	nwTowerRoofRitem->Mat = mMaterials["roofMat"].get();
	nwTowerRoofRitem->Geo = mGeometries["castleGeo"].get();
	nwTowerRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	nwTowerRoofRitem->IndexCount = nwTowerRoofRitem->Geo->DrawArgs["torusRoof"].IndexCount;
	nwTowerRoofRitem->StartIndexLocation = nwTowerRoofRitem->Geo->DrawArgs["torusRoof"].StartIndexLocation;
	nwTowerRoofRitem->BaseVertexLocation = nwTowerRoofRitem->Geo->DrawArgs["torusRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(nwTowerRoofRitem.get());
	mAllRitems.push_back(std::move(nwTowerRoofRitem));

	// Northeast Tower Roof
	auto neTowerRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX neRoofTransform = XMMatrixScaling(0.8f, 0.4f, 1.0f) * XMMatrixTranslation(30.0f, 11.0f, 30.0f);
	XMStoreFloat4x4(&neTowerRoofRitem->World, neRoofTransform);
	XMStoreFloat4x4(&neTowerRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	neTowerRoofRitem->ObjCBIndex = objCBIndex++;
	neTowerRoofRitem->Mat = mMaterials["roofMat"].get();
	neTowerRoofRitem->Geo = mGeometries["castleGeo"].get();
	neTowerRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	neTowerRoofRitem->IndexCount = neTowerRoofRitem->Geo->DrawArgs["torusRoof"].IndexCount;
	neTowerRoofRitem->StartIndexLocation = neTowerRoofRitem->Geo->DrawArgs["torusRoof"].StartIndexLocation;
	neTowerRoofRitem->BaseVertexLocation = neTowerRoofRitem->Geo->DrawArgs["torusRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(neTowerRoofRitem.get());
	mAllRitems.push_back(std::move(neTowerRoofRitem));

	// Southwest Tower Roof
	auto swTowerRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX swRoofTransform = XMMatrixScaling(0.8f, 0.4f, 1.0f) * XMMatrixTranslation(-30.0f, 11.0f, -30.0f);
	XMStoreFloat4x4(&swTowerRoofRitem->World, swRoofTransform);
	XMStoreFloat4x4(&swTowerRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	swTowerRoofRitem->ObjCBIndex = objCBIndex++;
	swTowerRoofRitem->Mat = mMaterials["roofMat"].get();
	swTowerRoofRitem->Geo = mGeometries["castleGeo"].get();
	swTowerRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	swTowerRoofRitem->IndexCount = swTowerRoofRitem->Geo->DrawArgs["torusRoof"].IndexCount;
	swTowerRoofRitem->StartIndexLocation = swTowerRoofRitem->Geo->DrawArgs["torusRoof"].StartIndexLocation;
	swTowerRoofRitem->BaseVertexLocation = swTowerRoofRitem->Geo->DrawArgs["torusRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(swTowerRoofRitem.get());
	mAllRitems.push_back(std::move(swTowerRoofRitem));

	// Southeast Tower Roof
	auto seTowerRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX seRoofTransform = XMMatrixScaling(0.8f, 0.3f, 1.0f) * XMMatrixTranslation(30.0f, 11.0f, -30.0f);
	XMStoreFloat4x4(&seTowerRoofRitem->World, seRoofTransform);
	XMStoreFloat4x4(&seTowerRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	seTowerRoofRitem->ObjCBIndex = objCBIndex++;
	seTowerRoofRitem->Mat = mMaterials["roofMat"].get();
	seTowerRoofRitem->Geo = mGeometries["castleGeo"].get();
	seTowerRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	seTowerRoofRitem->IndexCount = seTowerRoofRitem->Geo->DrawArgs["torusRoof"].IndexCount;
	seTowerRoofRitem->StartIndexLocation = seTowerRoofRitem->Geo->DrawArgs["torusRoof"].StartIndexLocation;
	seTowerRoofRitem->BaseVertexLocation = seTowerRoofRitem->Geo->DrawArgs["torusRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(seTowerRoofRitem.get());
	mAllRitems.push_back(std::move(seTowerRoofRitem));

	// KEEP PYRAMID ROOF
	auto keepPyramidRoofRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&keepPyramidRoofRitem->World, XMMatrixTranslation(0.0f, 25.0f, 0.0f));
	XMStoreFloat4x4(&keepPyramidRoofRitem->TexTransform, XMMatrixScaling(2.0f, 2.0f, 1.5f));
	keepPyramidRoofRitem->ObjCBIndex = objCBIndex++;
	keepPyramidRoofRitem->Mat = mMaterials["roofMat"].get();
	keepPyramidRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepPyramidRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepPyramidRoofRitem->IndexCount = keepPyramidRoofRitem->Geo->DrawArgs["keepPyramidRoof"].IndexCount;
	keepPyramidRoofRitem->StartIndexLocation = keepPyramidRoofRitem->Geo->DrawArgs["keepPyramidRoof"].StartIndexLocation;
	keepPyramidRoofRitem->BaseVertexLocation = keepPyramidRoofRitem->Geo->DrawArgs["keepPyramidRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepPyramidRoofRitem.get());
	mAllRitems.push_back(std::move(keepPyramidRoofRitem));

	// KEEP SIDE TOWERS
	// SW
	auto keepFrontLeftTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX frontLeftTowerScale = XMMatrixScaling(0.7f, 1.0f, 0.7f) * XMMatrixTranslation(-6.5f, 2.0f, -5.0f);
	XMStoreFloat4x4(&keepFrontLeftTowerRitem->World, frontLeftTowerScale);
	XMStoreFloat4x4(&keepFrontLeftTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	keepFrontLeftTowerRitem->ObjCBIndex = objCBIndex++;
	keepFrontLeftTowerRitem->Mat = mMaterials["brickMat"].get();
	keepFrontLeftTowerRitem->Geo = mGeometries["castleGeo"].get();
	keepFrontLeftTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFrontLeftTowerRitem->IndexCount = keepFrontLeftTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	keepFrontLeftTowerRitem->StartIndexLocation = keepFrontLeftTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	keepFrontLeftTowerRitem->BaseVertexLocation = keepFrontLeftTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepFrontLeftTowerRitem.get());
	mAllRitems.push_back(std::move(keepFrontLeftTowerRitem));

	// SE
	auto keepFrontRightTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX frontRightTowerScale = XMMatrixScaling(0.7f, 1.0f, 0.7f) * XMMatrixTranslation(6.5f, 2.0f, -5.0f);
	XMStoreFloat4x4(&keepFrontRightTowerRitem->World, frontRightTowerScale);
	XMStoreFloat4x4(&keepFrontRightTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	keepFrontRightTowerRitem->ObjCBIndex = objCBIndex++;
	keepFrontRightTowerRitem->Mat = mMaterials["brickMat"].get();
	keepFrontRightTowerRitem->Geo = mGeometries["castleGeo"].get();
	keepFrontRightTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFrontRightTowerRitem->IndexCount = keepFrontRightTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	keepFrontRightTowerRitem->StartIndexLocation = keepFrontRightTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	keepFrontRightTowerRitem->BaseVertexLocation = keepFrontRightTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepFrontRightTowerRitem.get());
	mAllRitems.push_back(std::move(keepFrontRightTowerRitem));

	// NW
	auto keepBackLeftTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX backLeftTowerScale = XMMatrixScaling(0.7f, 1.0f, 0.7f) * XMMatrixTranslation(-6.5f, 2.0f, 5.0f);
	XMStoreFloat4x4(&keepBackLeftTowerRitem->World, backLeftTowerScale);
	XMStoreFloat4x4(&keepBackLeftTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	keepBackLeftTowerRitem->ObjCBIndex = objCBIndex++;
	keepBackLeftTowerRitem->Mat = mMaterials["brickMat"].get();
	keepBackLeftTowerRitem->Geo = mGeometries["castleGeo"].get();
	keepBackLeftTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBackLeftTowerRitem->IndexCount = keepBackLeftTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	keepBackLeftTowerRitem->StartIndexLocation = keepBackLeftTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	keepBackLeftTowerRitem->BaseVertexLocation = keepBackLeftTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepBackLeftTowerRitem.get());
	mAllRitems.push_back(std::move(keepBackLeftTowerRitem));

	// NE
	auto keepBackRightTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX backRightTowerScale = XMMatrixScaling(0.7f, 1.0f, 0.7f) * XMMatrixTranslation(6.5f, 2.0f, 5.0f);
	XMStoreFloat4x4(&keepBackRightTowerRitem->World, backRightTowerScale);
	XMStoreFloat4x4(&keepBackRightTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	keepBackRightTowerRitem->ObjCBIndex = objCBIndex++;
	keepBackRightTowerRitem->Mat = mMaterials["brickMat"].get();
	keepBackRightTowerRitem->Geo = mGeometries["castleGeo"].get();
	keepBackRightTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBackRightTowerRitem->IndexCount = keepBackRightTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	keepBackRightTowerRitem->StartIndexLocation = keepBackRightTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	keepBackRightTowerRitem->BaseVertexLocation = keepBackRightTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepBackRightTowerRitem.get());
	mAllRitems.push_back(std::move(keepBackRightTowerRitem));

	// KEEP SIDE TOWER CONE ROOFS
	// SW
	auto keepFrontLeftConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX frontLeftConeTransform = XMMatrixScaling(0.8f, 1.7f, 0.7f) * XMMatrixTranslation(-6.5f, 16.0f, -5.0f);
	XMStoreFloat4x4(&keepFrontLeftConeRoofRitem->World, frontLeftConeTransform);
	XMStoreFloat4x4(&keepFrontLeftConeRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	keepFrontLeftConeRoofRitem->ObjCBIndex = objCBIndex++;
	keepFrontLeftConeRoofRitem->Mat = mMaterials["roofMat"].get();
	keepFrontLeftConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepFrontLeftConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFrontLeftConeRoofRitem->IndexCount = keepFrontLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	keepFrontLeftConeRoofRitem->StartIndexLocation = keepFrontLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	keepFrontLeftConeRoofRitem->BaseVertexLocation = keepFrontLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepFrontLeftConeRoofRitem.get());
	mAllRitems.push_back(std::move(keepFrontLeftConeRoofRitem));

	// SE
	auto keepFrontRightConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX frontRightConeTransform = XMMatrixScaling(0.8f, 1.7f, 0.7f) * XMMatrixTranslation(6.5f, 16.0f, -5.0f);
	XMStoreFloat4x4(&keepFrontRightConeRoofRitem->World, frontRightConeTransform);
	XMStoreFloat4x4(&keepFrontRightConeRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	keepFrontRightConeRoofRitem->ObjCBIndex = objCBIndex++;
	keepFrontRightConeRoofRitem->Mat = mMaterials["roofMat"].get();
	keepFrontRightConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepFrontRightConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFrontRightConeRoofRitem->IndexCount = keepFrontRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	keepFrontRightConeRoofRitem->StartIndexLocation = keepFrontRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	keepFrontRightConeRoofRitem->BaseVertexLocation = keepFrontRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepFrontRightConeRoofRitem.get());
	mAllRitems.push_back(std::move(keepFrontRightConeRoofRitem));

	// NW
	auto keepBackLeftConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX backLeftConeTransform = XMMatrixScaling(0.8f, 1.7f, 0.7f) * XMMatrixTranslation(-6.5f, 16.0f, 5.0f);
	XMStoreFloat4x4(&keepBackLeftConeRoofRitem->World, backLeftConeTransform);
	XMStoreFloat4x4(&keepBackLeftConeRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	keepBackLeftConeRoofRitem->ObjCBIndex = objCBIndex++;
	keepBackLeftConeRoofRitem->Mat = mMaterials["roofMat"].get();
	keepBackLeftConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepBackLeftConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBackLeftConeRoofRitem->IndexCount = keepBackLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	keepBackLeftConeRoofRitem->StartIndexLocation = keepBackLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	keepBackLeftConeRoofRitem->BaseVertexLocation = keepBackLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepBackLeftConeRoofRitem.get());
	mAllRitems.push_back(std::move(keepBackLeftConeRoofRitem));

	// NE
	auto keepBackRightConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX backRightConeTransform = XMMatrixScaling(0.8f, 1.7f, 0.7f) * XMMatrixTranslation(6.5f, 16.0f, 5.0f);
	XMStoreFloat4x4(&keepBackRightConeRoofRitem->World, backRightConeTransform);
	XMStoreFloat4x4(&keepBackRightConeRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	keepBackRightConeRoofRitem->ObjCBIndex = objCBIndex++;
	keepBackRightConeRoofRitem->Mat = mMaterials["roofMat"].get();
	keepBackRightConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepBackRightConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBackRightConeRoofRitem->IndexCount = keepBackRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	keepBackRightConeRoofRitem->StartIndexLocation = keepBackRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	keepBackRightConeRoofRitem->BaseVertexLocation = keepBackRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(keepBackRightConeRoofRitem.get());
	mAllRitems.push_back(std::move(keepBackRightConeRoofRitem));

	// DIAMOND SPIRE
	auto diamondSpireRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&diamondSpireRitem->World, XMMatrixTranslation(0.0f, 31.0f, 0.0f));
	XMStoreFloat4x4(&diamondSpireRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	diamondSpireRitem->ObjCBIndex = objCBIndex++;
	diamondSpireRitem->Mat = mMaterials["goldMat"].get();
	diamondSpireRitem->Geo = mGeometries["castleGeo"].get();
	diamondSpireRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	diamondSpireRitem->IndexCount = diamondSpireRitem->Geo->DrawArgs["diamondSpire"].IndexCount;
	diamondSpireRitem->StartIndexLocation = diamondSpireRitem->Geo->DrawArgs["diamondSpire"].StartIndexLocation;
	diamondSpireRitem->BaseVertexLocation = diamondSpireRitem->Geo->DrawArgs["diamondSpire"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(diamondSpireRitem.get());
	mAllRitems.push_back(std::move(diamondSpireRitem));

	// GATEHOUSE BASE 
	auto gatehouseRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gatehouseRitem->World, XMMatrixTranslation(0.0f, 4.0f, 31.0f));
	XMStoreFloat4x4(&gatehouseRitem->TexTransform, XMMatrixScaling(2.0f, 1.5f, 1.0f));
	gatehouseRitem->ObjCBIndex = objCBIndex++;
	gatehouseRitem->Mat = mMaterials["stoneMat"].get();
	gatehouseRitem->Geo = mGeometries["castleGeo"].get();
	gatehouseRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gatehouseRitem->IndexCount = gatehouseRitem->Geo->DrawArgs["gatehouse"].IndexCount;
	gatehouseRitem->StartIndexLocation = gatehouseRitem->Geo->DrawArgs["gatehouse"].StartIndexLocation;
	gatehouseRitem->BaseVertexLocation = gatehouseRitem->Geo->DrawArgs["gatehouse"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(gatehouseRitem.get());
	mAllRitems.push_back(std::move(gatehouseRitem));

	// GATE TOWERS 
	// Left gate tower
	auto leftGateTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX leftGateTowerScale = XMMatrixScaling(0.6f, 1.0f, 0.6f) * XMMatrixTranslation(-8.0f, 1.0f, 31.0f);
	XMStoreFloat4x4(&leftGateTowerRitem->World, leftGateTowerScale);
	XMStoreFloat4x4(&leftGateTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	leftGateTowerRitem->ObjCBIndex = objCBIndex++;
	leftGateTowerRitem->Mat = mMaterials["brickMat"].get();
	leftGateTowerRitem->Geo = mGeometries["castleGeo"].get();
	leftGateTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftGateTowerRitem->IndexCount = leftGateTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	leftGateTowerRitem->StartIndexLocation = leftGateTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	leftGateTowerRitem->BaseVertexLocation = leftGateTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(leftGateTowerRitem.get());
	mAllRitems.push_back(std::move(leftGateTowerRitem));

	// Right gate tower
	auto rightGateTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX rightGateTowerScale = XMMatrixScaling(0.6f, 1.0f, 0.6f) * XMMatrixTranslation(8.0f, 1.0f, 31.0f);
	XMStoreFloat4x4(&rightGateTowerRitem->World, rightGateTowerScale);
	XMStoreFloat4x4(&rightGateTowerRitem->TexTransform, XMMatrixScaling(1.0f, 2.0f, 1.0f));
	rightGateTowerRitem->ObjCBIndex = objCBIndex++;
	rightGateTowerRitem->Mat = mMaterials["brickMat"].get();
	rightGateTowerRitem->Geo = mGeometries["castleGeo"].get();
	rightGateTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightGateTowerRitem->IndexCount = rightGateTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	rightGateTowerRitem->StartIndexLocation = rightGateTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	rightGateTowerRitem->BaseVertexLocation = rightGateTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(rightGateTowerRitem.get());
	mAllRitems.push_back(std::move(rightGateTowerRitem));

	// GATE TOWER ROOFS
	// Left gate tower cone roof
	auto leftGateConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX leftGateConeTransform = XMMatrixScaling(0.6f, 1.2f, 1.0f) * XMMatrixTranslation(-8.0f, 13.5f, 31.0f);
	XMStoreFloat4x4(&leftGateConeRoofRitem->World, leftGateConeTransform);
	XMStoreFloat4x4(&leftGateConeRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	leftGateConeRoofRitem->ObjCBIndex = objCBIndex++;
	leftGateConeRoofRitem->Mat = mMaterials["roofMat"].get();
	leftGateConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	leftGateConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftGateConeRoofRitem->IndexCount = leftGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	leftGateConeRoofRitem->StartIndexLocation = leftGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	leftGateConeRoofRitem->BaseVertexLocation = leftGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(leftGateConeRoofRitem.get());
	mAllRitems.push_back(std::move(leftGateConeRoofRitem));

	// Right gate tower cone roof
	auto rightGateConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX rightGateConeTransform = XMMatrixScaling(0.6f, 1.2f, 1.0f) * XMMatrixTranslation(8.0f, 13.5f, 31.0f);
	XMStoreFloat4x4(&rightGateConeRoofRitem->World, rightGateConeTransform);
	XMStoreFloat4x4(&rightGateConeRoofRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	rightGateConeRoofRitem->ObjCBIndex = objCBIndex++;
	rightGateConeRoofRitem->Mat = mMaterials["roofMat"].get();
	rightGateConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	rightGateConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightGateConeRoofRitem->IndexCount = rightGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	rightGateConeRoofRitem->StartIndexLocation = rightGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	rightGateConeRoofRitem->BaseVertexLocation = rightGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(rightGateConeRoofRitem.get());
	mAllRitems.push_back(std::move(rightGateConeRoofRitem));

	// GATE COLUMNS (cylinders flanking gate opening)
	// Left gate column
	auto leftGateColumnRitem = std::make_unique<RenderItem>();
	XMMATRIX leftColumnTransform = XMMatrixScaling(0.5f, 1.0f, 0.5f) * XMMatrixTranslation(-3.0f, 4.0f, 34.0f);
	XMStoreFloat4x4(&leftGateColumnRitem->World, leftColumnTransform);
	XMStoreFloat4x4(&leftGateColumnRitem->TexTransform, XMMatrixScaling(0.5f, 1.0f, 0.5f));
	leftGateColumnRitem->ObjCBIndex = objCBIndex++;
	leftGateColumnRitem->Mat = mMaterials["stoneMat"].get();
	leftGateColumnRitem->Geo = mGeometries["castleGeo"].get();
	leftGateColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftGateColumnRitem->IndexCount = leftGateColumnRitem->Geo->DrawArgs["gateColumn"].IndexCount;
	leftGateColumnRitem->StartIndexLocation = leftGateColumnRitem->Geo->DrawArgs["gateColumn"].StartIndexLocation;
	leftGateColumnRitem->BaseVertexLocation = leftGateColumnRitem->Geo->DrawArgs["gateColumn"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(leftGateColumnRitem.get());
	mAllRitems.push_back(std::move(leftGateColumnRitem));

	// Right gate column
	auto rightGateColumnRitem = std::make_unique<RenderItem>();
	XMMATRIX rightColumnTransform = XMMatrixScaling(0.5f, 1.0f, 0.5f) * XMMatrixTranslation(3.0f, 4.0f, 34.0f);
	XMStoreFloat4x4(&rightGateColumnRitem->World, rightColumnTransform);
	XMStoreFloat4x4(&rightGateColumnRitem->TexTransform, XMMatrixScaling(0.5f, 1.0f, 0.5f));
	rightGateColumnRitem->ObjCBIndex = objCBIndex++;
	rightGateColumnRitem->Mat = mMaterials["stoneMat"].get();
	rightGateColumnRitem->Geo = mGeometries["castleGeo"].get();
	rightGateColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightGateColumnRitem->IndexCount = rightGateColumnRitem->Geo->DrawArgs["gateColumn"].IndexCount;
	rightGateColumnRitem->StartIndexLocation = rightGateColumnRitem->Geo->DrawArgs["gateColumn"].StartIndexLocation;
	rightGateColumnRitem->BaseVertexLocation = rightGateColumnRitem->Geo->DrawArgs["gateColumn"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(rightGateColumnRitem.get());
	mAllRitems.push_back(std::move(rightGateColumnRitem));

	// ARROW SLITS in gatehouse 
	// Left arrow slit
	auto gateArrowSlitLeftRitem = std::make_unique<RenderItem>();
	XMMATRIX gateArrowLeftTransform = XMMatrixRotationY(0.0f) * XMMatrixTranslation(-5.0f, 7.0f, 31.5f);
	XMStoreFloat4x4(&gateArrowSlitLeftRitem->World, gateArrowLeftTransform);
	XMStoreFloat4x4(&gateArrowSlitLeftRitem->TexTransform, XMMatrixScaling(0.5f, 1.0f, 1.0f));
	gateArrowSlitLeftRitem->ObjCBIndex = objCBIndex++;
	gateArrowSlitLeftRitem->Mat = mMaterials["darkStoneMat"].get();
	gateArrowSlitLeftRitem->Geo = mGeometries["castleGeo"].get();
	gateArrowSlitLeftRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gateArrowSlitLeftRitem->IndexCount = gateArrowSlitLeftRitem->Geo->DrawArgs["arrowSlit"].IndexCount;
	gateArrowSlitLeftRitem->StartIndexLocation = gateArrowSlitLeftRitem->Geo->DrawArgs["arrowSlit"].StartIndexLocation;
	gateArrowSlitLeftRitem->BaseVertexLocation = gateArrowSlitLeftRitem->Geo->DrawArgs["arrowSlit"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(gateArrowSlitLeftRitem.get());
	mAllRitems.push_back(std::move(gateArrowSlitLeftRitem));

	// Right arrow slit
	auto gateArrowSlitRightRitem = std::make_unique<RenderItem>();
	XMMATRIX gateArrowRightTransform = XMMatrixRotationY(0.0f) * XMMatrixTranslation(5.0f, 7.0f, 31.5f);
	XMStoreFloat4x4(&gateArrowSlitRightRitem->World, gateArrowRightTransform);
	XMStoreFloat4x4(&gateArrowSlitRightRitem->TexTransform, XMMatrixScaling(0.5f, 1.0f, 1.0f));
	gateArrowSlitRightRitem->ObjCBIndex = objCBIndex++;
	gateArrowSlitRightRitem->Mat = mMaterials["darkStoneMat"].get();
	gateArrowSlitRightRitem->Geo = mGeometries["castleGeo"].get();
	gateArrowSlitRightRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gateArrowSlitRightRitem->IndexCount = gateArrowSlitRightRitem->Geo->DrawArgs["arrowSlit"].IndexCount;
	gateArrowSlitRightRitem->StartIndexLocation = gateArrowSlitRightRitem->Geo->DrawArgs["arrowSlit"].StartIndexLocation;
	gateArrowSlitRightRitem->BaseVertexLocation = gateArrowSlitRightRitem->Geo->DrawArgs["arrowSlit"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(gateArrowSlitRightRitem.get());
	mAllRitems.push_back(std::move(gateArrowSlitRightRitem));

}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ShapesApp::GetStaticSamplers()
{  

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