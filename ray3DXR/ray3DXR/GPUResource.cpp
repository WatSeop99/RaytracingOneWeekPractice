#include "framework.h"
#include "Application.h"
#include "DescriptorAllocator.h"
#include "GPUResource.h"

bool ConstantBuffer::Initialize(Application* pApp, UINT dataSize, const void* pDATA)
{
	_ASSERT(pApp);
	_ASSERT(dataSize > 0);
	_ASSERT(!m_pResource);
	_ASSERT(!m_pMappedResource);
	_ASSERT(!m_pMemData);

	m_pApp = pApp;
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

	ID3D12Device5* pDevice = pApp->GetDevice();
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize);
	HRESULT hr = pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pResource));
	if (FAILED(hr))
	{
		return false;
	}

	DescriptorAllocator* pCBVAllocator = pApp->GetCBVSRVUAVAllocator();
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_pResource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_BufferSize;

	pCBVAllocator->AllocDescriptorHandle(&m_CPUHandle);
	pDevice->CreateConstantBufferView(&cbvDesc, m_CPUHandle);
	m_GPUAddress = cbvDesc.BufferLocation;

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

		DescriptorAllocator* pCBVAllocator = m_pApp->GetCBVSRVUAVAllocator();
		pCBVAllocator->FreeDescriptorHandle(m_CPUHandle);
		m_CPUHandle = {};
		m_GPUAddress = UINT64_MAX;

		m_pResource->Release();
		m_pResource = nullptr;
	}
	if (m_pMemData)
	{
		free(m_pMemData);
		m_pMemData = nullptr;
	}
	m_BufferSize = 0;
	m_pApp = nullptr;

	return true;
}

bool ConstantBuffer::Upload()
{
	_ASSERT(m_pMappedResource);
	_ASSERT(m_pMemData);

	memcpy(m_pMappedResource, m_pMemData, m_DataSize);

	return true;
}

bool StructuredBuffer::Initialize(Application* pApp, UINT sizePerData, UINT numData, const void* pDATA)
{
	_ASSERT(pApp);
	_ASSERT(sizePerData > 0);
	_ASSERT(numData > 0);
	_ASSERT(!m_pResource);
	_ASSERT(!m_pMappedResource);
	_ASSERT(!m_pMemData);

	if (sizePerData % 16 != 0)
	{
		return false;
	}
	m_pApp = pApp;
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

	ID3D12Device5* pDevice = m_pApp->GetDevice();
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize);
	HRESULT hr = pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pResource));
	if (FAILED(hr))
	{
		return false;
	}

	DescriptorAllocator* pSRVAllocator = m_pApp->GetCBVSRVUAVAllocator();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numData;
	srvDesc.Buffer.StructureByteStride = sizePerData;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	pSRVAllocator->AllocDescriptorHandle(&m_CPUHandle);
	pDevice->CreateShaderResourceView(m_pResource, &srvDesc, m_CPUHandle);
	m_GPUAddress = m_pResource->GetGPUVirtualAddress();

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

		DescriptorAllocator* pSRVAllocator = m_pApp->GetCBVSRVUAVAllocator();
		pSRVAllocator->FreeDescriptorHandle(m_CPUHandle);
		m_CPUHandle = {};
		m_GPUAddress = UINT64_MAX;

		m_pResource->Release();
		m_pResource = nullptr;
	}
	if (m_pMemData)
	{
		free(m_pMemData);
		m_pMemData = nullptr;
	}
	m_BufferSize = 0;
	m_pApp = nullptr;

	return true;
}

bool StructuredBuffer::Upload()
{
	_ASSERT(m_pMappedResource);
	_ASSERT(m_pMemData);

	memcpy(m_pMappedResource, m_pMemData, m_BufferSize);

	return true;
}
