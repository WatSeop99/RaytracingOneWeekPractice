#pragma once

#include "Camera.h"
#include "CommonTypes.h"
#include "StepTimer.h"

enum D3DDeviceOption
{
	DeviceOption_AllowTearing = 0x1,
	DeviceOption_RequireTearingSupport = 0x2
};

class Renderer final
{
private:
	static const UINT MAX_BACK_BUFFER_COUNT = 2;

public:
	Renderer() = default;
	~Renderer() { Cleanup(); }

	bool Initialize(HINSTANCE hInstance, WNDPROC pfnWndProc, UINT width, UINT height);
	bool Cleanup();

	void Run();

	void Update();
	void Render();

	void ChangeSize(UINT width, UINT height);

	// Mouse process.
	void ProcessMosuseMove(int x, int y);
	inline void SetMouseLButton(bool bSet) { m_Mouse.bMouseLeftButton = bSet; }
	inline void SetMouseRButton(bool bSet) { m_Mouse.bMouseRightButton = bSet; }

	// Keyboard process.
	inline void SetKeyboardKey(WPARAM wParam, bool bSet) { m_Keyboard.bPressed[wParam] = bSet; }

private:
	bool InitializeDXGIAdapter();
	bool InitializeD3DDeviceResources();
	bool InitScene();

	bool CreateWindowDependentedResources();
	void ReleaseWindowDependentedResources();
	bool CreateDeviceDependentResources();
	bool CreateConstantBuffers();
	bool CreateRaytracingOutput();

	bool CreateRootSignature();
	bool CreateRaytracingPipelineStateObject();
	void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* pRaytracingPipeline);

	bool BuildGeometry();
	bool BuildAccelerationStructures();
	bool BuildShaderTables();

	bool AllocateUploadBuffer(void* pData, UINT64 dataSize, ID3D12Resource** ppOutResource, const WCHAR* pszResourceName);
	bool AllocateUAVBuffer(UINT64 bufferSize, ID3D12Resource** ppOutResource, D3D12_RESOURCE_STATES initialResourceState, const WCHAR* pszResourceName);
	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, UINT descriptorIndexToUse = UINT_MAX);

	UINT CreateBufferSRV(D3DBuffer* pOutBuffer, UINT numElements, UINT elementSize);

	void UpdateCameraMatrices();

	void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT);
	void DoRaytracing();
	void CopyRaytracingOutputToBackbuffer();
	void ExecuteCommandList();
	void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET);

	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC* pDesc, ID3D12RootSignature** ppRootSig);

	void Fence();
	void WaitForGPU();

private:
	HWND m_hMainWindow = nullptr;
	UINT m_Width = 0;
	UINT m_Height = 0;
	float m_AspectRatio = 0.0f;

	bool m_bEnableUI = true;

	IDXGIFactory7* m_pDXGIFactory = nullptr;
	IDXGIAdapter4* m_pDXGIAdapter = nullptr;
	IDXGISwapChain4* m_pSwapChain = nullptr;
	UINT m_AdapterID = UINT_MAX;
	WCHAR m_szAdapterDescription[512] = { 0, };

	DXGI_FORMAT m_BackBufferFormat = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT m_DepthBufferFormat = DXGI_FORMAT_UNKNOWN;
	UINT m_Option = DeviceOption_AllowTearing | DeviceOption_RequireTearingSupport;
	UINT m_BackBufferCount = 0;
	UINT m_FrameIndex = 0;
	D3D_FEATURE_LEVEL m_D3DFeatureLevel = D3D_FEATURE_LEVEL_11_0;

	ID3D12Device14* m_pDevice = nullptr;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	ID3D12GraphicsCommandList10* m_ppCommandLists[MAX_BACK_BUFFER_COUNT] = { nullptr, };
	ID3D12CommandAllocator* m_ppCommandAllocators[MAX_BACK_BUFFER_COUNT] = { nullptr, };

	ID3D12Resource2* m_ppRenderTargets[MAX_BACK_BUFFER_COUNT] = { nullptr, };
	ID3D12Resource2* m_pDepthStencil = nullptr;

	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_FenceValues[MAX_BACK_BUFFER_COUNT] = { 0, };
	HANDLE m_hFenceEvent = nullptr;

	ID3D12DescriptorHeap* m_pRTVDescriptorHeap = nullptr;
	ID3D12DescriptorHeap* m_pDSVDescriptorHeap = nullptr;
	ID3D12DescriptorHeap* m_pCBVSRVUAVDescriptorHeap = nullptr;
	UINT m_NumRTVDescriptorAlloced = 0;
	UINT m_NumDSVDescriptorAlloced = 0;
	UINT m_NumCBVSRVUAVDescriptorAlloced = 0;
	
	UINT m_RaytracingOutputResourceUAVDescriptorHeapIndex = 0xFFFFFFFF;
	ID3D12Resource2* m_pRaytracingOutput = nullptr;
	ID3D12Resource2* m_pTopLevelAccelerationStructure = nullptr;

	ID3D12StateObject* m_pStateObject = nullptr;
	ID3D12RootSignature* m_pRaytracingGlobalRootSignature = nullptr;
	ID3D12RootSignature* m_pRaytracingLocalRootSignature = nullptr;
	ID3D12Resource2* m_pMissShaderTable = nullptr;
	ID3D12Resource2* m_pHitGroupShaderTable = nullptr;
	ID3D12Resource2* m_pRayGenShaderTable = nullptr;

	AlignedSceneConstantBuffer* m_pMappedConstantData = nullptr;
	ID3D12Resource2* m_pFrameConstants = nullptr;
	FrameBuffer m_FrameCB[MAX_BACK_BUFFER_COUNT] = {};

	std::vector<Geometry> m_Geometry;
	std::map<std::string, BLASData> m_BLASTypeCache;
	UINT m_InstanceContributionToHitGroupIndex = 0;
	UINT m_HitgroupShaderTableSize = 0;
	D3DBuffer m_VertexBuffer = INIT_BUFFER;
	D3DBuffer m_IndexBuffer = INIT_BUFFER;

	StepTimer m_Timer;
	float m_CurRotationAngleRad = 0.0f;
	DirectX::XMVECTOR m_Eye;
	DirectX::XMVECTOR m_At;
	DirectX::XMVECTOR m_Up;

	Camera m_Camera;
	Keyboard m_Keyboard;
	Mouse m_Mouse;
};

