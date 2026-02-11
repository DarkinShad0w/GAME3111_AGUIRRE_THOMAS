/** @file Week4-6-ShapeComplete.cpp
 *  @brief Shape Practice Solution.
 *
 *  Place all of the scene geometry in one big vertex and index buffer.
 * Then use the DrawIndexedInstanced method to draw one object at a time ((as the
 * world matrix needs to be changed between objects)
 *
 *   Controls:
 *   Hold down '1' key to view scene in wireframe mode.
 *   Hold the left mouse button down and move the mouse to rotate.
 *   Hold the right mouse button down and move the mouse to zoom in and out.
 *
 *  @author Hooman Salamat
 */


#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will winMain
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
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
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildDescriptorHeaps();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

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

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBufferViews();
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

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

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
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
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

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)mOpaqueRitems.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	UINT numDescriptors = (objCount + 1) * gNumFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	mPassCbvOffset = objCount * gNumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(&mCbvHeap)));
}

void ShapesApp::BuildConstantBufferViews()
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	UINT objCount = (UINT)mOpaqueRitems.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = mPassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void ShapesApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
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
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\VS.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\PS.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
		vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGreen);
	}

	// Keep Foundation - dark gray
	for (size_t i = 0; i < keepFoundation.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = keepFoundation.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGray);
	}

	// Keep Body - light gray
	for (size_t i = 0; i < keepBody.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = keepBody.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::LightGray);
	}

	// Outer Walls Long - stone gray
	for (size_t i = 0; i < outerWallLong.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = outerWallLong.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f);
	}

	// Outer Walls Short - stone gray
	for (size_t i = 0; i < outerWallShort.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = outerWallShort.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f);
	}

	// Hexagonal Towers - medium gray
	for (size_t i = 0; i < hexTower.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = hexTower.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Gray);
	}

	// Torus Roofs - dark brown
	for (size_t i = 0; i < torusRoof.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = torusRoof.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(0.4f, 0.2f, 0.1f, 1.0f);
	}

	// Keep Pyramid Roof - dark red
	for (size_t i = 0; i < keepPyramidRoof.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = keepPyramidRoof.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkRed);
	}

	// Keep Cone Roofs - dark brown
	for (size_t i = 0; i < keepConeRoof.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = keepConeRoof.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(0.4f, 0.2f, 0.1f, 1.0f);
	}

	// Diamond Spire - gold
	for (size_t i = 0; i < diamondSpire.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = diamondSpire.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Gold);
	}

	// Arrow Slits - black
	for (size_t i = 0; i < arrowSlit.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = arrowSlit.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Black);
	}

	// Gable Wedges - medium gray
	for (size_t i = 0; i < gableWedge.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = gableWedge.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	}

	// Gate Columns - Dark Gray
	for (size_t i = 0; i < gateColumn.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = gateColumn.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	}

	// Gatehouse - Medium Gray
	for (size_t i = 0; i < gatehouse.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = gatehouse.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
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

void ShapesApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	// PSO for opaque objects.

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

	// PSO for opaque wireframe objects.

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}



void ShapesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size()));
	}
}



void ShapesApp::BuildRenderItems()
{
	// Clear existing render items
	mAllRitems.clear();
	mOpaqueRitems.clear();

	// GROUND
	auto groundRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&groundRitem->World, XMMatrixTranslation(0.0f, -0.5f, 0.0f));
	groundRitem->ObjCBIndex = 0;
	groundRitem->Geo = mGeometries["castleGeo"].get();
	groundRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	groundRitem->IndexCount = groundRitem->Geo->DrawArgs["ground"].IndexCount;
	groundRitem->StartIndexLocation = groundRitem->Geo->DrawArgs["ground"].StartIndexLocation;
	groundRitem->BaseVertexLocation = groundRitem->Geo->DrawArgs["ground"].BaseVertexLocation;
	mAllRitems.push_back(std::move(groundRitem));

	// FOUNDATION
	auto keepFoundationRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&keepFoundationRitem->World, XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	keepFoundationRitem->ObjCBIndex = 1;
	keepFoundationRitem->Geo = mGeometries["castleGeo"].get();
	keepFoundationRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFoundationRitem->IndexCount = keepFoundationRitem->Geo->DrawArgs["keepFoundation"].IndexCount;
	keepFoundationRitem->StartIndexLocation = keepFoundationRitem->Geo->DrawArgs["keepFoundation"].StartIndexLocation;
	keepFoundationRitem->BaseVertexLocation = keepFoundationRitem->Geo->DrawArgs["keepFoundation"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepFoundationRitem));

	// BODY
	auto keepBodyRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&keepBodyRitem->World, XMMatrixTranslation(0.0f, 6.0f, 0.0f));
	keepBodyRitem->ObjCBIndex = 2;
	keepBodyRitem->Geo = mGeometries["castleGeo"].get();
	keepBodyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBodyRitem->IndexCount = keepBodyRitem->Geo->DrawArgs["keepBody"].IndexCount;
	keepBodyRitem->StartIndexLocation = keepBodyRitem->Geo->DrawArgs["keepBody"].StartIndexLocation;
	keepBodyRitem->BaseVertexLocation = keepBodyRitem->Geo->DrawArgs["keepBody"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepBodyRitem));

	// OUTER WALLS
	UINT objCBIndex = 3;

	// North Wall (facing +Z)
	auto northWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&northWallRitem->World, XMMatrixTranslation(0.0f, 3.0f, 30.0f));
	northWallRitem->ObjCBIndex = objCBIndex++;
	northWallRitem->Geo = mGeometries["castleGeo"].get();
	northWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	northWallRitem->IndexCount = northWallRitem->Geo->DrawArgs["outerWallLong"].IndexCount;
	northWallRitem->StartIndexLocation = northWallRitem->Geo->DrawArgs["outerWallLong"].StartIndexLocation;
	northWallRitem->BaseVertexLocation = northWallRitem->Geo->DrawArgs["outerWallLong"].BaseVertexLocation;
	mAllRitems.push_back(std::move(northWallRitem));

	// South Wall (facing -Z)
	auto southWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&southWallRitem->World, XMMatrixTranslation(0.0f, 3.0f, -30.0f));
	southWallRitem->ObjCBIndex = objCBIndex++;
	southWallRitem->Geo = mGeometries["castleGeo"].get();
	southWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	southWallRitem->IndexCount = southWallRitem->Geo->DrawArgs["outerWallLong"].IndexCount;
	southWallRitem->StartIndexLocation = southWallRitem->Geo->DrawArgs["outerWallLong"].StartIndexLocation;
	southWallRitem->BaseVertexLocation = southWallRitem->Geo->DrawArgs["outerWallLong"].BaseVertexLocation;
	mAllRitems.push_back(std::move(southWallRitem));

	// East Wall (facing +X)
	auto eastWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&eastWallRitem->World, XMMatrixTranslation(30.0f, 3.0f, 0.0f));
	eastWallRitem->ObjCBIndex = objCBIndex++;
	eastWallRitem->Geo = mGeometries["castleGeo"].get();
	eastWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	eastWallRitem->IndexCount = eastWallRitem->Geo->DrawArgs["outerWallShort"].IndexCount;
	eastWallRitem->StartIndexLocation = eastWallRitem->Geo->DrawArgs["outerWallShort"].StartIndexLocation;
	eastWallRitem->BaseVertexLocation = eastWallRitem->Geo->DrawArgs["outerWallShort"].BaseVertexLocation;
	mAllRitems.push_back(std::move(eastWallRitem));

	// West Wall (facing -X)
	auto westWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&westWallRitem->World, XMMatrixTranslation(-30.0f, 3.0f, 0.0f));
	westWallRitem->ObjCBIndex = objCBIndex++;
	westWallRitem->Geo = mGeometries["castleGeo"].get();
	westWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	westWallRitem->IndexCount = westWallRitem->Geo->DrawArgs["outerWallShort"].IndexCount;
	westWallRitem->StartIndexLocation = westWallRitem->Geo->DrawArgs["outerWallShort"].StartIndexLocation;
	westWallRitem->BaseVertexLocation = westWallRitem->Geo->DrawArgs["outerWallShort"].BaseVertexLocation;
	mAllRitems.push_back(std::move(westWallRitem));

	// HEXAGONAL CORNER TOWERS
	// Northwest Tower (-X, +Z)
	auto nwTowerRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&nwTowerRitem->World, XMMatrixTranslation(-30.0f, 1.0f, 30.0f));
	nwTowerRitem->ObjCBIndex = objCBIndex++;
	nwTowerRitem->Geo = mGeometries["castleGeo"].get();
	nwTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	nwTowerRitem->IndexCount = nwTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	nwTowerRitem->StartIndexLocation = nwTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	nwTowerRitem->BaseVertexLocation = nwTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(nwTowerRitem));

	// Northeast Tower (+X, +Z)
	auto neTowerRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&neTowerRitem->World, XMMatrixTranslation(30.0f, 1.0f, 30.0f));
	neTowerRitem->ObjCBIndex = objCBIndex++;
	neTowerRitem->Geo = mGeometries["castleGeo"].get();
	neTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	neTowerRitem->IndexCount = neTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	neTowerRitem->StartIndexLocation = neTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	neTowerRitem->BaseVertexLocation = neTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(neTowerRitem));

	// Southwest Tower (-X, -Z)
	auto swTowerRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&swTowerRitem->World, XMMatrixTranslation(-30.0f, 1.0f, -30.0f));
	swTowerRitem->ObjCBIndex = objCBIndex++;
	swTowerRitem->Geo = mGeometries["castleGeo"].get();
	swTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	swTowerRitem->IndexCount = swTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	swTowerRitem->StartIndexLocation = swTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	swTowerRitem->BaseVertexLocation = swTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(swTowerRitem));

	// Southeast Tower (+X, -Z)
	auto seTowerRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&seTowerRitem->World, XMMatrixTranslation(30.0f, 1.0f, -30.0f));
	seTowerRitem->ObjCBIndex = objCBIndex++;
	seTowerRitem->Geo = mGeometries["castleGeo"].get();
	seTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	seTowerRitem->IndexCount = seTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	seTowerRitem->StartIndexLocation = seTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	seTowerRitem->BaseVertexLocation = seTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(seTowerRitem));

	// TORUS ROOFS ON TOWERS
	// Northwest Tower Roof
	auto nwTowerRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX nwRoofTransform = XMMatrixScaling(0.8f, 0.4f, 1.0f) * XMMatrixTranslation(-30.0f, 11.0f, 30.0f);
	XMStoreFloat4x4(&nwTowerRoofRitem->World, nwRoofTransform);
	nwTowerRoofRitem->ObjCBIndex = objCBIndex++;
	nwTowerRoofRitem->Geo = mGeometries["castleGeo"].get();
	nwTowerRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	nwTowerRoofRitem->IndexCount = nwTowerRoofRitem->Geo->DrawArgs["torusRoof"].IndexCount;
	nwTowerRoofRitem->StartIndexLocation = nwTowerRoofRitem->Geo->DrawArgs["torusRoof"].StartIndexLocation;
	nwTowerRoofRitem->BaseVertexLocation = nwTowerRoofRitem->Geo->DrawArgs["torusRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(nwTowerRoofRitem));

	// Northeast Tower Roof
	auto neTowerRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX neRoofTransform = XMMatrixScaling(0.8f, 0.4f, 1.0f) * XMMatrixTranslation(30.0f, 11.0f, 30.0f);
	XMStoreFloat4x4(&neTowerRoofRitem->World, neRoofTransform);
	neTowerRoofRitem->ObjCBIndex = objCBIndex++;
	neTowerRoofRitem->Geo = mGeometries["castleGeo"].get();
	neTowerRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	neTowerRoofRitem->IndexCount = neTowerRoofRitem->Geo->DrawArgs["torusRoof"].IndexCount;
	neTowerRoofRitem->StartIndexLocation = neTowerRoofRitem->Geo->DrawArgs["torusRoof"].StartIndexLocation;
	neTowerRoofRitem->BaseVertexLocation = neTowerRoofRitem->Geo->DrawArgs["torusRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(neTowerRoofRitem));

	// Southwest Tower Roof
	auto swTowerRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX swRoofTransform = XMMatrixScaling(0.8f, 0.4f, 1.0f) * XMMatrixTranslation(-30.0f, 11.0f, -30.0f);
	XMStoreFloat4x4(&swTowerRoofRitem->World, swRoofTransform);
	swTowerRoofRitem->ObjCBIndex = objCBIndex++;
	swTowerRoofRitem->Geo = mGeometries["castleGeo"].get();
	swTowerRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	swTowerRoofRitem->IndexCount = swTowerRoofRitem->Geo->DrawArgs["torusRoof"].IndexCount;
	swTowerRoofRitem->StartIndexLocation = swTowerRoofRitem->Geo->DrawArgs["torusRoof"].StartIndexLocation;
	swTowerRoofRitem->BaseVertexLocation = swTowerRoofRitem->Geo->DrawArgs["torusRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(swTowerRoofRitem));

	// Southeast Tower Roof
	auto seTowerRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX seRoofTransform = XMMatrixScaling(0.8f, 0.3f, 1.0f) * XMMatrixTranslation(30.0f, 11.0f, -30.0f);
	XMStoreFloat4x4(&seTowerRoofRitem->World, seRoofTransform);
	seTowerRoofRitem->ObjCBIndex = objCBIndex++;
	seTowerRoofRitem->Geo = mGeometries["castleGeo"].get();
	seTowerRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	seTowerRoofRitem->IndexCount = seTowerRoofRitem->Geo->DrawArgs["torusRoof"].IndexCount;
	seTowerRoofRitem->StartIndexLocation = seTowerRoofRitem->Geo->DrawArgs["torusRoof"].StartIndexLocation;
	seTowerRoofRitem->BaseVertexLocation = seTowerRoofRitem->Geo->DrawArgs["torusRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(seTowerRoofRitem));

	// KEEP PYRAMID ROOF
	auto keepPyramidRoofRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&keepPyramidRoofRitem->World, XMMatrixTranslation(0.0f, 25.0f, 0.0f));
	keepPyramidRoofRitem->ObjCBIndex = objCBIndex++;
	keepPyramidRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepPyramidRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepPyramidRoofRitem->IndexCount = keepPyramidRoofRitem->Geo->DrawArgs["keepPyramidRoof"].IndexCount;
	keepPyramidRoofRitem->StartIndexLocation = keepPyramidRoofRitem->Geo->DrawArgs["keepPyramidRoof"].StartIndexLocation;
	keepPyramidRoofRitem->BaseVertexLocation = keepPyramidRoofRitem->Geo->DrawArgs["keepPyramidRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepPyramidRoofRitem));

	// KEEP SIDE TOWERS
	auto keepFrontLeftTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX frontLeftTowerScale = XMMatrixScaling(0.7f, 1.0f, 0.7f) * XMMatrixTranslation(-6.5f, 2.0f, -5.0f);
	XMStoreFloat4x4(&keepFrontLeftTowerRitem->World, frontLeftTowerScale);
	keepFrontLeftTowerRitem->ObjCBIndex = objCBIndex++;
	keepFrontLeftTowerRitem->Geo = mGeometries["castleGeo"].get();
	keepFrontLeftTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFrontLeftTowerRitem->IndexCount = keepFrontLeftTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	keepFrontLeftTowerRitem->StartIndexLocation = keepFrontLeftTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	keepFrontLeftTowerRitem->BaseVertexLocation = keepFrontLeftTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepFrontLeftTowerRitem));

	auto keepFrontRightTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX frontRightTowerScale = XMMatrixScaling(0.7f, 1.0f, 0.7f) * XMMatrixTranslation(6.5f, 2.0f, -5.0f);
	XMStoreFloat4x4(&keepFrontRightTowerRitem->World, frontRightTowerScale);
	keepFrontRightTowerRitem->ObjCBIndex = objCBIndex++;
	keepFrontRightTowerRitem->Geo = mGeometries["castleGeo"].get();
	keepFrontRightTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFrontRightTowerRitem->IndexCount = keepFrontRightTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	keepFrontRightTowerRitem->StartIndexLocation = keepFrontRightTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	keepFrontRightTowerRitem->BaseVertexLocation = keepFrontRightTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepFrontRightTowerRitem));

	auto keepBackLeftTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX backLeftTowerScale = XMMatrixScaling(0.7f, 1.0f, 0.7f) * XMMatrixTranslation(-6.5f, 2.0f, 5.0f);
	XMStoreFloat4x4(&keepBackLeftTowerRitem->World, backLeftTowerScale);
	keepBackLeftTowerRitem->ObjCBIndex = objCBIndex++;
	keepBackLeftTowerRitem->Geo = mGeometries["castleGeo"].get();
	keepBackLeftTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBackLeftTowerRitem->IndexCount = keepBackLeftTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	keepBackLeftTowerRitem->StartIndexLocation = keepBackLeftTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	keepBackLeftTowerRitem->BaseVertexLocation = keepBackLeftTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepBackLeftTowerRitem));

	auto keepBackRightTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX backRightTowerScale = XMMatrixScaling(0.7f, 1.0f, 0.7f) * XMMatrixTranslation(6.5f, 2.0f, 5.0f);
	XMStoreFloat4x4(&keepBackRightTowerRitem->World, backRightTowerScale);
	keepBackRightTowerRitem->ObjCBIndex = objCBIndex++;
	keepBackRightTowerRitem->Geo = mGeometries["castleGeo"].get();
	keepBackRightTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBackRightTowerRitem->IndexCount = keepBackRightTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	keepBackRightTowerRitem->StartIndexLocation = keepBackRightTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	keepBackRightTowerRitem->BaseVertexLocation = keepBackRightTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepBackRightTowerRitem));

	// KEEP SIDE TOWER CONE ROOFS
	auto keepFrontLeftConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX frontLeftConeTransform = XMMatrixScaling(0.8f, 1.7f, 0.7f) * XMMatrixTranslation(-6.5f, 16.0f, -5.0f);
	XMStoreFloat4x4(&keepFrontLeftConeRoofRitem->World, frontLeftConeTransform);
	keepFrontLeftConeRoofRitem->ObjCBIndex = objCBIndex++;
	keepFrontLeftConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepFrontLeftConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFrontLeftConeRoofRitem->IndexCount = keepFrontLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	keepFrontLeftConeRoofRitem->StartIndexLocation = keepFrontLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	keepFrontLeftConeRoofRitem->BaseVertexLocation = keepFrontLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepFrontLeftConeRoofRitem));

	auto keepFrontRightConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX frontRightConeTransform = XMMatrixScaling(0.8f, 1.7f, 0.7f) * XMMatrixTranslation(6.5f, 16.0f, -5.0f);
	XMStoreFloat4x4(&keepFrontRightConeRoofRitem->World, frontRightConeTransform);
	keepFrontRightConeRoofRitem->ObjCBIndex = objCBIndex++;
	keepFrontRightConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepFrontRightConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepFrontRightConeRoofRitem->IndexCount = keepFrontRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	keepFrontRightConeRoofRitem->StartIndexLocation = keepFrontRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	keepFrontRightConeRoofRitem->BaseVertexLocation = keepFrontRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepFrontRightConeRoofRitem));

	auto keepBackLeftConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX backLeftConeTransform = XMMatrixScaling(0.8f, 1.7f, 0.7f) * XMMatrixTranslation(-6.5f, 16.0f, 5.0f);
	XMStoreFloat4x4(&keepBackLeftConeRoofRitem->World, backLeftConeTransform);
	keepBackLeftConeRoofRitem->ObjCBIndex = objCBIndex++;
	keepBackLeftConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepBackLeftConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBackLeftConeRoofRitem->IndexCount = keepBackLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	keepBackLeftConeRoofRitem->StartIndexLocation = keepBackLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	keepBackLeftConeRoofRitem->BaseVertexLocation = keepBackLeftConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepBackLeftConeRoofRitem));

	auto keepBackRightConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX backRightConeTransform = XMMatrixScaling(0.8f, 1.7f, 0.7f) * XMMatrixTranslation(6.5f, 16.0f, 5.0f);
	XMStoreFloat4x4(&keepBackRightConeRoofRitem->World, backRightConeTransform);
	keepBackRightConeRoofRitem->ObjCBIndex = objCBIndex++;
	keepBackRightConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	keepBackRightConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	keepBackRightConeRoofRitem->IndexCount = keepBackRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	keepBackRightConeRoofRitem->StartIndexLocation = keepBackRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	keepBackRightConeRoofRitem->BaseVertexLocation = keepBackRightConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(keepBackRightConeRoofRitem));

	// DIAMOND SPIRE
	auto diamondSpireRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&diamondSpireRitem->World, XMMatrixTranslation(0.0f, 31.0f, 0.0f));
	diamondSpireRitem->ObjCBIndex = objCBIndex++;
	diamondSpireRitem->Geo = mGeometries["castleGeo"].get();
	diamondSpireRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	diamondSpireRitem->IndexCount = diamondSpireRitem->Geo->DrawArgs["diamondSpire"].IndexCount;
	diamondSpireRitem->StartIndexLocation = diamondSpireRitem->Geo->DrawArgs["diamondSpire"].StartIndexLocation;
	diamondSpireRitem->BaseVertexLocation = diamondSpireRitem->Geo->DrawArgs["diamondSpire"].BaseVertexLocation;
	mAllRitems.push_back(std::move(diamondSpireRitem));

	// GATEHOUSE BASE 
	auto gatehouseRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gatehouseRitem->World, XMMatrixTranslation(0.0f, 4.0f, 31.0f));
	gatehouseRitem->ObjCBIndex = objCBIndex++;
	gatehouseRitem->Geo = mGeometries["castleGeo"].get();
	gatehouseRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gatehouseRitem->IndexCount = gatehouseRitem->Geo->DrawArgs["gatehouse"].IndexCount;
	gatehouseRitem->StartIndexLocation = gatehouseRitem->Geo->DrawArgs["gatehouse"].StartIndexLocation;
	gatehouseRitem->BaseVertexLocation = gatehouseRitem->Geo->DrawArgs["gatehouse"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gatehouseRitem));

	// GATE TOWERS 
	// Left gate tower
	auto leftGateTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX leftGateTowerScale = XMMatrixScaling(0.6f, 1.0f, 0.6f) * XMMatrixTranslation(-8.0f, 1.0f, 31.0f);
	XMStoreFloat4x4(&leftGateTowerRitem->World, leftGateTowerScale);
	leftGateTowerRitem->ObjCBIndex = objCBIndex++;
	leftGateTowerRitem->Geo = mGeometries["castleGeo"].get();
	leftGateTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftGateTowerRitem->IndexCount = leftGateTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	leftGateTowerRitem->StartIndexLocation = leftGateTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	leftGateTowerRitem->BaseVertexLocation = leftGateTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(leftGateTowerRitem));

	// Right gate tower
	auto rightGateTowerRitem = std::make_unique<RenderItem>();
	XMMATRIX rightGateTowerScale = XMMatrixScaling(0.6f, 1.0f, 0.6f) * XMMatrixTranslation(8.0f, 1.0f, 31.0f);
	XMStoreFloat4x4(&rightGateTowerRitem->World, rightGateTowerScale);
	rightGateTowerRitem->ObjCBIndex = objCBIndex++;
	rightGateTowerRitem->Geo = mGeometries["castleGeo"].get();
	rightGateTowerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightGateTowerRitem->IndexCount = rightGateTowerRitem->Geo->DrawArgs["hexTower"].IndexCount;
	rightGateTowerRitem->StartIndexLocation = rightGateTowerRitem->Geo->DrawArgs["hexTower"].StartIndexLocation;
	rightGateTowerRitem->BaseVertexLocation = rightGateTowerRitem->Geo->DrawArgs["hexTower"].BaseVertexLocation;
	mAllRitems.push_back(std::move(rightGateTowerRitem));

	// GATE TOWER ROOFS
	// Left gate tower cone roof
	auto leftGateConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX leftGateConeTransform = XMMatrixScaling(0.6f, 1.2f, 1.0f) * XMMatrixTranslation(-8.0f, 13.5f, 31.0f); 
	XMStoreFloat4x4(&leftGateConeRoofRitem->World, leftGateConeTransform);
	leftGateConeRoofRitem->ObjCBIndex = objCBIndex++;
	leftGateConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	leftGateConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftGateConeRoofRitem->IndexCount = leftGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	leftGateConeRoofRitem->StartIndexLocation = leftGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	leftGateConeRoofRitem->BaseVertexLocation = leftGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(leftGateConeRoofRitem));

	// Right gate tower cone roof
	auto rightGateConeRoofRitem = std::make_unique<RenderItem>();
	XMMATRIX rightGateConeTransform = XMMatrixScaling(0.6f, 1.2f, 1.0f) * XMMatrixTranslation(8.0f, 13.5f, 31.0f); 
	XMStoreFloat4x4(&rightGateConeRoofRitem->World, rightGateConeTransform);
	rightGateConeRoofRitem->ObjCBIndex = objCBIndex++;
	rightGateConeRoofRitem->Geo = mGeometries["castleGeo"].get();
	rightGateConeRoofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightGateConeRoofRitem->IndexCount = rightGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].IndexCount;
	rightGateConeRoofRitem->StartIndexLocation = rightGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].StartIndexLocation;
	rightGateConeRoofRitem->BaseVertexLocation = rightGateConeRoofRitem->Geo->DrawArgs["keepConeRoof"].BaseVertexLocation;
	mAllRitems.push_back(std::move(rightGateConeRoofRitem));

	// GATE COLUMNS (cylinders flanking gate opening)
	// Left gate column
	auto leftGateColumnRitem = std::make_unique<RenderItem>();
	XMMATRIX leftColumnTransform = XMMatrixScaling(0.5f, 1.0f, 0.5f) * XMMatrixTranslation(-3.0f, 4.0f, 34.0f);
	XMStoreFloat4x4(&leftGateColumnRitem->World, leftColumnTransform);
	leftGateColumnRitem->ObjCBIndex = objCBIndex++;
	leftGateColumnRitem->Geo = mGeometries["castleGeo"].get();
	leftGateColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftGateColumnRitem->IndexCount = leftGateColumnRitem->Geo->DrawArgs["gateColumn"].IndexCount;
	leftGateColumnRitem->StartIndexLocation = leftGateColumnRitem->Geo->DrawArgs["gateColumn"].StartIndexLocation;
	leftGateColumnRitem->BaseVertexLocation = leftGateColumnRitem->Geo->DrawArgs["gateColumn"].BaseVertexLocation;
	mAllRitems.push_back(std::move(leftGateColumnRitem));

	// Right gate column
	auto rightGateColumnRitem = std::make_unique<RenderItem>();
	XMMATRIX rightColumnTransform = XMMatrixScaling(0.5f, 1.0f, 0.5f) * XMMatrixTranslation(3.0f, 4.0f, 34.0f);
	XMStoreFloat4x4(&rightGateColumnRitem->World, rightColumnTransform);
	rightGateColumnRitem->ObjCBIndex = objCBIndex++;
	rightGateColumnRitem->Geo = mGeometries["castleGeo"].get();
	rightGateColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightGateColumnRitem->IndexCount = rightGateColumnRitem->Geo->DrawArgs["gateColumn"].IndexCount;
	rightGateColumnRitem->StartIndexLocation = rightGateColumnRitem->Geo->DrawArgs["gateColumn"].StartIndexLocation;
	rightGateColumnRitem->BaseVertexLocation = rightGateColumnRitem->Geo->DrawArgs["gateColumn"].BaseVertexLocation;
	mAllRitems.push_back(std::move(rightGateColumnRitem));

	// ARROW SLITS in gatehouse 
	// Left arrow slit
	auto gateArrowSlitLeftRitem = std::make_unique<RenderItem>();
	XMMATRIX gateArrowLeftTransform = XMMatrixRotationY(0.0f) * XMMatrixTranslation(-5.0f, 7.0f, 31.5f);
	XMStoreFloat4x4(&gateArrowSlitLeftRitem->World, gateArrowLeftTransform);
	gateArrowSlitLeftRitem->ObjCBIndex = objCBIndex++;
	gateArrowSlitLeftRitem->Geo = mGeometries["castleGeo"].get();
	gateArrowSlitLeftRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gateArrowSlitLeftRitem->IndexCount = gateArrowSlitLeftRitem->Geo->DrawArgs["arrowSlit"].IndexCount;
	gateArrowSlitLeftRitem->StartIndexLocation = gateArrowSlitLeftRitem->Geo->DrawArgs["arrowSlit"].StartIndexLocation;
	gateArrowSlitLeftRitem->BaseVertexLocation = gateArrowSlitLeftRitem->Geo->DrawArgs["arrowSlit"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gateArrowSlitLeftRitem));

	// Right arrow slit
	auto gateArrowSlitRightRitem = std::make_unique<RenderItem>();
	XMMATRIX gateArrowRightTransform = XMMatrixRotationY(0.0f) * XMMatrixTranslation(5.0f, 7.0f, 31.5f);
	XMStoreFloat4x4(&gateArrowSlitRightRitem->World, gateArrowRightTransform);
	gateArrowSlitRightRitem->ObjCBIndex = objCBIndex++;
	gateArrowSlitRightRitem->Geo = mGeometries["castleGeo"].get();
	gateArrowSlitRightRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gateArrowSlitRightRitem->IndexCount = gateArrowSlitRightRitem->Geo->DrawArgs["arrowSlit"].IndexCount;
	gateArrowSlitRightRitem->StartIndexLocation = gateArrowSlitRightRitem->Geo->DrawArgs["arrowSlit"].StartIndexLocation;
	gateArrowSlitRightRitem->BaseVertexLocation = gateArrowSlitRightRitem->Geo->DrawArgs["arrowSlit"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gateArrowSlitRightRitem));


	// All the render items are opaque.
	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	// For each render item...

	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];
		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.

		UINT cbvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjCBIndex;

		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());

		cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);
		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}