#include "framework.h"
#include "GPUResources.h"

void GPUUploadBuffer::Allocate(ID3D12Device* pDevice, UINT bufferSize, LPCWSTR pszRresourceName)
{
	_ASSERT(pDevice);
	_ASSERT(bufferSize > 0);

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);

	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
	if (FAILED(pDevice->CreateCommittedResource(&uploadHeapProperties,
												D3D12_HEAP_FLAG_NONE,
												&bufferDesc,
												D3D12_RESOURCE_STATE_GENERIC_READ,
												nullptr,
												IID_PPV_ARGS(&m_pResource))))
	{
		__debugbreak();
	}

	if (pszRresourceName)
	{
		m_pResource->SetName(pszRresourceName);
	}
}

BYTE* GPUUploadBuffer::MapCPUWriteOnly()
{
	BYTE* pMappedData = nullptr;
	CD3DX12_RANGE readRange(0, 0);
	if (FAILED(m_pResource->Map(0, &readRange, reinterpret_cast<void**>(&pMappedData))))
	{
#ifdef _DEBUG
		__debugbreak();
#endif
	}
	return pMappedData;
}
