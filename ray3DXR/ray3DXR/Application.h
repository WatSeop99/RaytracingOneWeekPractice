#pragma once

#include "BaseForm.h"

struct FrameObject
{
	ID3D12CommandAllocator* pCommandAllocator;
	ID3D12Resource* pSwapChainBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle;
};
struct HeapData
{
	ID3D12DescriptorHeap* pHeap;
	UINT UsedEntries;
};

class Application final : public BaseForm
{
public:
	static const UINT SWAP_CHAIN_BUFFER = 3;
	static const UINT RTV_HEAP_SIZE = 3;
	static const UINT CBV_SRV_UAV_HEAP_SIZE = 2;

public:
	Application(HINSTANCE hInstance) : BaseForm(hInstance) {}
	~Application();

	void OnLoad() override;
	void OnFrameRender() override;
	void OnShutdown() override;

private:
	void InitDXR();
	
	UINT BeginFrame();
	void EndFrame();

	void CreateAccelerationStructures();
	void CreateRTPipelineState();
	void CreateShaderTable();
	void CreateShaderResources();

	ID3D12Device5* CreateDevice(IDXGIFactory4* pDXGIFactory);
	ID3D12CommandQueue* CreateCommandQueue(ID3D12Device5* pDevice);
	IDXGISwapChain3* CreateSwapChain(IDXGIFactory4* pFactory, HWND hwnd, UINT width, UINT height, DXGI_FORMAT format, ID3D12CommandQueue* pCommandQueue);
	ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device5* pDevice, UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool bShaderVisible);
	D3D12_CPU_DESCRIPTOR_HANDLE CreateRTV(ID3D12Device5* pDevice, ID3D12Resource* pResource, ID3D12DescriptorHeap* pHeap, UINT* outUsedHeapEntries, DXGI_FORMAT format);

private:
	ID3D12Device5* m_pDevice = nullptr;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	IDXGISwapChain3* m_pSwapChain = nullptr;
	ID3D12GraphicsCommandList4* m_pCommandList = nullptr;
	ID3D12Fence* m_pFence = nullptr;
	HANDLE m_hFenceEvent = nullptr;
	UINT64 m_FenceValue = 0;

	FrameObject m_FrameObjects[SWAP_CHAIN_BUFFER] = { 0, };
	UINT m_FrameIndex = 0;

	HeapData m_RTVHeap = { 0, };

	ID3D12Resource* m_pVertexBuffer = nullptr;
	ID3D12Resource* m_pTopLevelAS = nullptr;
	ID3D12Resource* m_pBottomLevelAS = nullptr;
	UINT64 m_TlasSize = 0;

	ID3D12StateObject* m_pPipelineState = nullptr;
	ID3D12RootSignature* m_pEmptyRootSignature = nullptr;

	ID3D12Resource* m_pShaderTable = nullptr;
	UINT m_ShaderTableEntrySize = 0;

	ID3D12Resource* m_pOutputResource = nullptr;
	ID3D12DescriptorHeap* m_pCBVSRVUAVHeap = nullptr;
};
