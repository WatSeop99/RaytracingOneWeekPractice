#pragma once

struct Buffer;
class Application;

class ResourceManager
{
public:
	ResourceManager() = default;
	~ResourceManager() { Cleanup(); }

	bool Initialize(Application* pApp);
	bool Cleanup();

	Buffer* CreateVertexBuffer(UINT sizePerVertex, UINT numVertex, const void* pINIT_DATA);
	Buffer* CreateIndexBuffer(UINT sizePerIndex, UINT numIndex, const void* pINIT_DATA);

private:
	ID3D12Device5* m_pDevice = nullptr;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
	ID3D12GraphicsCommandList4* m_pCommandList = nullptr;
	
	ID3D12Fence* m_pFence = nullptr;
	HANDLE m_hFenceEvent = nullptr;
	UINT64 m_FenceValue = 0;

	// DO NOT Release directly.
	Application* m_pApp = nullptr;
};
