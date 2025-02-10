#pragma once

class DynamicDescriptorPool
{
public:
	DynamicDescriptorPool() = default;
	~DynamicDescriptorPool() { Cleanup(); }

	void Initialize(ID3D12Device5* pDevice, UINT maxDescriptorCount);

	HRESULT AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptorHandle, UINT descriptorCount);

	void Reset();

	void Cleanup();

	inline ID3D12DescriptorHeap* GetDescriptorHeap() { return m_pDescriptorHeap; }

private:
	ID3D12Device* m_pDevice = nullptr;
	ID3D12DescriptorHeap* m_pDescriptorHeap = nullptr;
	UINT64 m_AllocatedDescriptorCount = 0;
	UINT64 m_MaxDescriptorCount = 0;
	UINT m_CBVSRVUAVDescriptorSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE m_CPUDescriptorHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_GPUDescriptorHandle = {};
};
