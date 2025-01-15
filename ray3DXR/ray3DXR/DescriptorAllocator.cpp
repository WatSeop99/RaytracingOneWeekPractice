#include "framework.h"
#include "DescriptorAllocator.h"

bool DescriptorAllocator::Initialize(ID3D12Device5* pDevice, UINT maxCount, D3D12_DESCRIPTOR_HEAP_FLAGS flag, D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	_ASSERT(pDevice);
	_ASSERT(maxCount > 0);
	_ASSERT(heapType < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);

	m_pDevice = pDevice;
	m_pDevice->AddRef();

	D3D12_DESCRIPTOR_HEAP_DESC commonHeapDesc = {};
	commonHeapDesc.NumDescriptors = maxCount;
	commonHeapDesc.Type = heapType;
	commonHeapDesc.Flags = flag;

	if (FAILED(m_pDevice->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&m_pHeap))))
	{
		return false;
	}

	m_IndexCreator.Initialize(maxCount);
	m_DescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(heapType);

	return true;
}

bool DescriptorAllocator::Cleanup()
{
#ifdef _DEBUG
	m_IndexCreator.Check();
#endif

	m_DescriptorSize = 0;
	if (m_pHeap)
	{
		m_pHeap->Release();
		m_pHeap = nullptr;
	}
	if (m_pDevice)
	{
		m_pDevice->Release();
		m_pDevice = nullptr;
	}

	return true;
}

UINT DescriptorAllocator::AllocDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUHandle)
{
	_ASSERT(pOutCPUHandle);

	UINT index = m_IndexCreator.Alloc();
	if (index == -1)
	{
		__debugbreak();
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHandle(m_pHeap->GetCPUDescriptorHandleForHeapStart(), index, m_DescriptorSize);
	*pOutCPUHandle = DescriptorHandle;

	return index;
}

void DescriptorAllocator::FreeDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle)
{
	D3D12_CPU_DESCRIPTOR_HANDLE base = m_pHeap->GetCPUDescriptorHandleForHeapStart();
#ifdef _DEBUG
	if (base.ptr > CPUHandle.ptr)
	{
		__debugbreak();
	}
#endif

	UINT index = (UINT)(CPUHandle.ptr - base.ptr) / m_DescriptorSize;
	m_IndexCreator.Free(index);
}

bool DescriptorAllocator::Check(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle)
{
	bool bRet = true;

	D3D12_CPU_DESCRIPTOR_HANDLE base = m_pHeap->GetCPUDescriptorHandleForHeapStart();
	if (base.ptr > CPUHandle.ptr)
	{
		bRet = false;
	}

	return bRet;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetGPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle)
{
	D3D12_CPU_DESCRIPTOR_HANDLE base = m_pHeap->GetCPUDescriptorHandleForHeapStart();
#ifdef _DEBUG
	if (base.ptr > CPUHandle.ptr)
	{
		__debugbreak();
	}
#endif
	UINT index = (UINT)(CPUHandle.ptr - base.ptr) / m_DescriptorSize;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GPUHandle(m_pHeap->GetGPUDescriptorHandleForHeapStart(), index, m_DescriptorSize);

	return GPUHandle;
}
