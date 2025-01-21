#pragma once

class Application;

class ConstantBuffer
{
public:
	ConstantBuffer() = default;
	~ConstantBuffer() { Cleanup(); }

	bool Initialize(Application* pApp, UINT dataSize, const void* pDATA);
	bool Cleanup();

	bool Upload();

	inline ID3D12Resource* GetResource() { return m_pResource; }
	inline void* GetDataMem() { return m_pMemData; }
	inline UINT GetBufferSize() { return m_BufferSize; }
	inline UINT GetDataSize() { return m_DataSize; }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() { return m_CPUHandle; }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() { return m_GPUAddress; }

private:
	ID3D12Resource* m_pResource = nullptr;
	BYTE* m_pMappedResource = nullptr;
	void* m_pMemData = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle = {};
	D3D12_GPU_VIRTUAL_ADDRESS m_GPUAddress = UINT64_MAX;

	UINT m_BufferSize = 0;
	UINT m_DataSize = 0;

	// DO NOT Release directly.
	Application* m_pApp = nullptr;
};

class StructuredBuffer
{
public:
	StructuredBuffer() = default;
	~StructuredBuffer() { Cleanup(); }

	bool Initialize(Application* pApp, UINT sizePerData, UINT numData, const void* pDATA);
	bool Cleanup();

	bool Upload();

	inline ID3D12Resource* GetResource() { return m_pResource; }
	inline void* GetDataMem() { return m_pMemData; }
	inline UINT GetBufferSize() { return m_BufferSize; }
	inline UINT GetNumData() { return m_NumData; }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() { return m_CPUHandle; }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() { return m_GPUAddress; }

private:
	ID3D12Resource* m_pResource = nullptr;
	void* m_pMappedResource = nullptr;
	void* m_pMemData = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle = {};
	D3D12_GPU_VIRTUAL_ADDRESS m_GPUAddress = UINT64_MAX;

	UINT m_BufferSize = 0;
	UINT m_NumData = 0;

	// DO NOT Release directly.
	Application* m_pApp = nullptr;
};
