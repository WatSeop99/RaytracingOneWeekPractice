#include "framework.h"
#include "AccelerationStructure.h"

void AccelerationStructure::CleanupD3DResources()
{
	if (m_pAccelerationStructure)
	{
		m_pAccelerationStructure->Release();
		m_pAccelerationStructure = nullptr;
	}
}

bool AccelerationStructure::AllocateResource(ID3D12Device5* pDevice)
{
	_ASSERT(pDevice);

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_PrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	HRESULT hr = pDevice->CreateCommittedResource(&uploadHeapProperties,
												  D3D12_HEAP_FLAG_NONE,
												  &bufferDesc,
												  D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
												  nullptr,
												  IID_PPV_ARGS(&m_pAccelerationStructure));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	if (wcslen(m_szName) > 0)
	{
		m_pAccelerationStructure->SetName(m_szName);
	}
	
	return true;
}

void BottomLevelAccelerationStructureGeometry::SetName(const WCHAR* pszName)
{
	_ASSERT(pszName);
	_ASSERT(wcslen(pszName) > 0);

	wcsncpy_s(szName, MAX_PATH, pszName, wcslen(pszName));
}
