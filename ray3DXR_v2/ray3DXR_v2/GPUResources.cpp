#include "framework.h"
#include "GPUResources.h"

GPUUploadBuffer::~GPUUploadBuffer()
{
	if (m_pResource)
	{
		m_pResource->Unmap(0, nullptr);
		m_pResource->Release();
		m_pResource = nullptr;
	}
}

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

void ShaderRecord::CopyTo(void* pDest) const
{
	_ASSERT(ShaderIdentifier.Ptr);
	_ASSERT(ShaderIdentifier.Size > 0);
	_ASSERT(pDest);

	BYTE* pByteDest = (BYTE*)pDest;
	memcpy(pByteDest, ShaderIdentifier.Ptr, ShaderIdentifier.Size);
	if (LocalRootArguments.Ptr)
	{
		memcpy(pByteDest + ShaderIdentifier.Size, LocalRootArguments.Ptr, LocalRootArguments.Size);
	}
}

ShaderTable::ShaderTable(ID3D12Device* pDevice, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR pszResourceName)
{
	if (pszResourceName)
	{ 
		wcsncpy_s(m_szName, 512, pszResourceName, wcslen(pszResourceName));
	}

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
	Allocate(pDevice, bufferSize, pszResourceName);
	m_pMappedShaderRecords = MapCPUWriteOnly();
	if (!m_pMappedShaderRecords)
	{
		__debugbreak();
	}
}

void ShaderTable::PushBack(const ShaderRecord& shaderRecord)
{
	if (m_ShaderRecords.size() >= m_ShaderRecords.capacity())
	{
		__debugbreak();
	}

	m_ShaderRecords.push_back(shaderRecord);
	shaderRecord.CopyTo(m_pMappedShaderRecords);
	m_pMappedShaderRecords += m_ShaderRecordSize;
}

void ShaderTable::DebugPrint(std::unordered_map<void*, std::wstring>& shaderIdToStringMap)
{
	std::wstringstream wstr;
	wstr << L"|--------------------------------------------------------------------\n";
	wstr << L"|Shader table - " << m_szName << L": "
		<< m_ShaderRecordSize << L" | "
		<< m_ShaderRecords.size() * m_ShaderRecordSize << L" bytes\n";

	for (SIZE_T i = 0, size = m_ShaderRecords.size(); i < size; i++)
	{
		wstr << L"| [" << i << L"]: ";
		wstr << shaderIdToStringMap[m_ShaderRecords[i].ShaderIdentifier.Ptr] << L", ";
		wstr << m_ShaderRecords[i].ShaderIdentifier.Size << L" + " << m_ShaderRecords[i].LocalRootArguments.Size << L" bytes \n";
	}
	wstr << L"|--------------------------------------------------------------------\n";
	wstr << L"\n";
	OutputDebugStringW(wstr.str().c_str());
}
