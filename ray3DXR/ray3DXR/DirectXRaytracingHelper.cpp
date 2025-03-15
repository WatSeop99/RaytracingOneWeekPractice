#include "framework.h"
#include "DXSampleHelper.h"
#include "DirectXRaytracingHelper.h"

void ShaderRecord::CopyTo(void* pDest) const
{
	BYTE* pByteDest = (BYTE*)pDest;
	memcpy(pByteDest, ShaderIdentifier.Ptr, ShaderIdentifier.Size);
	if (LocalRootArguments.Ptr)
	{
		memcpy(pByteDest + ShaderIdentifier.Size, LocalRootArguments.Ptr, LocalRootArguments.Size);
	}
}

ShaderTable::ShaderTable(ID3D12Device* pDevice, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName)
{
	wcsncpy_s(m_Name, 512, resourceName, wcslen(resourceName));

#ifndef ROUND_UP
#define ROUND_UP(v, powerOf2Alignment) (((v) + (powerOf2Alignment)-1) & ~((powerOf2Alignment)-1))
#endif

	const UINT SHADER_IDENTIFIER_SIZE = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
#define ALIGN(alignment, num) ((((num) + alignment - 1) / alignment) * alignment)
	const UINT OFFSET_TO_MATERIAL_CONSTANTS = ALIGN(sizeof(UINT32), SHADER_IDENTIFIER_SIZE);
	const UINT SHADER_RECORD_SIZE_IN_BYTES = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, OFFSET_TO_MATERIAL_CONSTANTS + shaderRecordSize);

	m_ShaderRecordSize = SHADER_RECORD_SIZE_IN_BYTES;

	m_ShaderRecords.reserve(numShaderRecords);
	UINT bufferSize = numShaderRecords * SHADER_RECORD_SIZE_IN_BYTES;
	Allocate(pDevice, bufferSize, resourceName);
	m_pMappedShaderRecords = MapCpuWriteOnly();
}

void ShaderTable::PushBack(const ShaderRecord& shaderRecord)
{
	ThrowIfFalse(m_ShaderRecords.size() < m_ShaderRecords.capacity());
	m_ShaderRecords.push_back(shaderRecord);
	shaderRecord.CopyTo(m_pMappedShaderRecords);
	m_pMappedShaderRecords += m_ShaderRecordSize;
}

void ShaderTable::DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap)
{
	std::wstringstream wstr;
	wstr << L"|--------------------------------------------------------------------\n";
	wstr << L"|Shader table - " << m_Name << L": "
		<< m_ShaderRecordSize << L" | "
		<< m_ShaderRecords.size() * m_ShaderRecordSize << L" bytes\n";

	for (UINT i = 0; i < m_ShaderRecords.size(); i++)
	{
		wstr << L"| [" << i << L"]: ";
		wstr << shaderIdToStringMap[m_ShaderRecords[i].ShaderIdentifier.Ptr] << L", ";
		wstr << m_ShaderRecords[i].ShaderIdentifier.Size << L" + " << m_ShaderRecords[i].LocalRootArguments.Size << L" bytes \n";
	}
	wstr << L"|--------------------------------------------------------------------\n";
	wstr << L"\n";
	OutputDebugStringW(wstr.str().c_str());
}


void AllocateUAVBuffer(ID3D12Device* pDevice, UINT64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState, const WCHAR* resourceName)
{
	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(pDevice->CreateCommittedResource(&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		initialResourceState,
		nullptr,
		IID_PPV_ARGS(ppResource)));
	if (resourceName)
	{
		(*ppResource)->SetName(resourceName);
	}
}

void AllocateUploadBuffer(ID3D12Device* pDevice, void* pData, UINT64 datasize, ID3D12Resource** ppResource, const WCHAR* resourceName)
{
	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(datasize);
	ThrowIfFailed(pDevice->CreateCommittedResource(&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(ppResource)));
	if (resourceName)
	{
		(*ppResource)->SetName(resourceName);
	}

	void* pMappedData = nullptr;
	(*ppResource)->Map(0, nullptr, &pMappedData);
	memcpy(pMappedData, pData, datasize);
	(*ppResource)->Unmap(0, nullptr);
}

void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* pDesc)
{
	std::wstringstream wstr;
	wstr << L"\n";
	wstr << L"--------------------------------------------------------------------\n";
	wstr << L"| D3D12 State Object 0x" << (const void*)pDesc << L": ";
	if (pDesc->Type == D3D12_STATE_OBJECT_TYPE_COLLECTION) wstr << L"Collection\n";
	if (pDesc->Type == D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE) wstr << L"Raytracing Pipeline\n";

	auto ExportTree = [](UINT depth, UINT numExports, const D3D12_EXPORT_DESC* exports)
		{
			std::wostringstream woss;
			for (UINT i = 0; i < numExports; i++)
			{
				woss << L"|";
				if (depth > 0)
				{
					for (UINT j = 0; j < 2 * depth - 1; j++) woss << L" ";
				}
				woss << L" [" << i << L"]: ";
				if (exports[i].ExportToRename) woss << exports[i].ExportToRename << L" --> ";
				woss << exports[i].Name << L"\n";
			}
			return woss.str();
		};

	for (UINT i = 0; i < pDesc->NumSubobjects; i++)
	{
		wstr << L"| [" << i << L"]: ";
		switch (pDesc->pSubobjects[i].Type)
		{
		case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
			wstr << L"Global Root Signature 0x" << pDesc->pSubobjects[i].pDesc << L"\n";
			break;

		case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
			wstr << L"Local Root Signature 0x" << pDesc->pSubobjects[i].pDesc << L"\n";
			break;

		case D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK:
			wstr << L"Node Mask: 0x" << std::hex << std::setfill(L'0') << std::setw(8) << *(const UINT*)pDesc->pSubobjects[i].pDesc << std::setw(0) << std::dec << L"\n";
			break;

		case D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY:
		{
			wstr << L"DXIL Library 0x";
			const D3D12_DXIL_LIBRARY_DESC* pLib = (const D3D12_DXIL_LIBRARY_DESC*)pDesc->pSubobjects[i].pDesc;
			wstr << pLib->DXILLibrary.pShaderBytecode << L", " << pLib->DXILLibrary.BytecodeLength << L" bytes\n";
			wstr << ExportTree(1, pLib->NumExports, pLib->pExports);
			break;
		}

		case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
		{
			wstr << L"Existing Library 0x";
			const D3D12_EXISTING_COLLECTION_DESC* pCollection = (const D3D12_EXISTING_COLLECTION_DESC*)(pDesc->pSubobjects[i].pDesc);
			wstr << pCollection->pExistingCollection << L"\n";
			wstr << ExportTree(1, pCollection->NumExports, pCollection->pExports);
			break;
		}

		case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
		{
			wstr << L"Subobject to Exports Association (Subobject [";
			const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION* pAssociation = (const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*)pDesc->pSubobjects[i].pDesc;
			UINT index = static_cast<UINT>(pAssociation->pSubobjectToAssociate - pDesc->pSubobjects);
			wstr << index << L"])\n";
			for (UINT j = 0; j < pAssociation->NumExports; j++)
			{
				wstr << L"|  [" << j << L"]: " << pAssociation->pExports[j] << L"\n";
			}
			break;
		}

		case D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
		{
			wstr << L"DXIL Subobjects to Exports Association (";
			const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION* pAssociation = (const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION*)pDesc->pSubobjects[i].pDesc;
			wstr << pAssociation->SubobjectToAssociate << L")\n";
			for (UINT j = 0; j < pAssociation->NumExports; j++)
			{
				wstr << L"|  [" << j << L"]: " << pAssociation->pExports[j] << L"\n";
			}
			break;
		}

		case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG:
		{
			wstr << L"Raytracing Shader Config\n";
			const D3D12_RAYTRACING_SHADER_CONFIG* pConfig = (const D3D12_RAYTRACING_SHADER_CONFIG*)pDesc->pSubobjects[i].pDesc;
			wstr << L"|  [0]: Max Payload Size: " << pConfig->MaxPayloadSizeInBytes << L" bytes\n";
			wstr << L"|  [1]: Max Attribute Size: " << pConfig->MaxAttributeSizeInBytes << L" bytes\n";
			break;
		}

		case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG:
		{
			wstr << L"Raytracing Pipeline Config\n";
			const D3D12_RAYTRACING_PIPELINE_CONFIG* pConfig = (const D3D12_RAYTRACING_PIPELINE_CONFIG*)pDesc->pSubobjects[i].pDesc;
			wstr << L"|  [0]: Max Recursion Depth: " << pConfig->MaxTraceRecursionDepth << L"\n";
			break;
		}

		case D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP:
		{
			wstr << L"Hit Group (";
			const D3D12_HIT_GROUP_DESC* pHitGroup = (const D3D12_HIT_GROUP_DESC*)pDesc->pSubobjects[i].pDesc;
			wstr << (pHitGroup->HitGroupExport ? pHitGroup->HitGroupExport : L"[none]") << L")\n";
			wstr << L"|  [0]: Any Hit Import: " << (pHitGroup->AnyHitShaderImport ? pHitGroup->AnyHitShaderImport : L"[none]") << L"\n";
			wstr << L"|  [1]: Closest Hit Import: " << (pHitGroup->ClosestHitShaderImport ? pHitGroup->ClosestHitShaderImport : L"[none]") << L"\n";
			wstr << L"|  [2]: Intersection Import: " << (pHitGroup->IntersectionShaderImport ? pHitGroup->IntersectionShaderImport : L"[none]") << L"\n";
			break;
		}
		}
		wstr << L"|--------------------------------------------------------------------\n";
	}
	wstr << L"\n";
	OutputDebugStringW(wstr.str().c_str());
}

bool IsDirectXRaytracingSupported(IDXGIAdapter1* pAdapter)
{
	HRESULT hr;
	ID3D12Device* pTestDevice = nullptr;
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

	hr = D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pTestDevice));
	if (FAILED(hr))
	{
		return false;
	}

	hr = pTestDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData));
	ULONG ref = pTestDevice->Release();
	pTestDevice = nullptr;
	if (FAILED(hr))
	{
		return false;
	}

	return (featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
}