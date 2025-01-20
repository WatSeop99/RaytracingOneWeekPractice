#include "framework.h"
#include "GPUResource.h"

bool ConstantBuffer::Initialize(ID3D12Device5* pDevice, UINT dataSize, const void* pDATA)
{
	_ASSERT(pDevice);
	_ASSERT(dataSize > 0);
	_ASSERT(!m_pResource);
	_ASSERT(!m_pMappedResource);
	_ASSERT(!m_pMemData);

	m_BufferSize = (dataSize + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
	m_DataSize = dataSize;

	m_pMemData = malloc(dataSize);
	if (!m_pMemData)
	{
		return false;
	}
	if (pDATA)
	{
		memcpy(m_pMemData, pDATA, dataSize);
	}
	else
	{
		ZeroMemory(m_pMemData, dataSize);
	}

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize);
	HRESULT hr = pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pResource));
	if (FAILED(hr))
	{
		return false;
	}

	CD3DX12_RANGE readRange(0, 0);
	m_pResource->Map(0, &readRange, (void**)&m_pMappedResource);
	if (!m_pMappedResource)
	{
		return false;
	}

	Upload();

	return true;
}

bool ConstantBuffer::Cleanup()
{
	if (m_pResource)
	{
		m_pResource->Unmap(0, nullptr);
		m_pMappedResource = nullptr;

		m_pResource->Release();
		m_pResource = nullptr;
	}
	if (m_pMemData)
	{
		free(m_pMemData);
		m_pMemData = nullptr;
	}
	m_BufferSize = 0;

	return true;
}

bool ConstantBuffer::Upload()
{
	_ASSERT(m_pMappedResource);
	_ASSERT(m_pMemData);

	memcpy(m_pMappedResource, m_pMemData, m_DataSize);

	return true;
}

bool StructuredBuffer::Initialize(ID3D12Device5* pDevice, UINT sizePerData, UINT numData, const void* pDATA)
{
	_ASSERT(pDevice);
	_ASSERT(sizePerData > 0);
	_ASSERT(numData > 0);
	_ASSERT(!m_pResource);
	_ASSERT(!m_pMappedResource);
	_ASSERT(!m_pMemData);

	if (sizePerData % 16 != 0)
	{
		return false;
	}
	m_BufferSize = sizePerData * numData;
	m_NumData = numData;

	m_pMemData = malloc(m_BufferSize);
	if (!m_pMemData)
	{
		return false;
	}
	if (pDATA)
	{
		memcpy(m_pMemData, pDATA, m_BufferSize);
	}
	else
	{
		ZeroMemory(m_pMemData, m_BufferSize);
	}

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize);
	HRESULT hr = pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pResource));
	if (FAILED(hr))
	{
		return false;
	}

	CD3DX12_RANGE readRange(0, 0);
	m_pResource->Map(0, &readRange, (void**)&m_pMappedResource);
	if (!m_pMappedResource)
	{
		return false;
	}

	Upload();

	return true;
}

bool StructuredBuffer::Cleanup()
{
	if (m_pResource)
	{
		m_pResource->Unmap(0, nullptr);
		m_pMappedResource = nullptr;

		m_pResource->Release();
		m_pResource = nullptr;
	}
	m_BufferSize = 0;

	return true;
}

bool StructuredBuffer::Upload()
{
	_ASSERT(m_pMappedResource);
	_ASSERT(m_pMemData);

	memcpy(m_pMappedResource, m_pMemData, m_BufferSize);

	return true;
}
