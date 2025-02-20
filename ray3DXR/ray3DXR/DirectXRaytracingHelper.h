//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

// Shader record = {{Shader ID}, {RootArguments}}
class ShaderRecord
{
public:
	class PointerWithSize
	{
	public:
		PointerWithSize() = default;
		PointerWithSize(void* ptr, UINT size) : Ptr(ptr), Size(size) {};

	public:
		void* Ptr = nullptr;
		UINT Size = 0;
	};

public:
	ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize) : ShaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
	{}

	ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize) :
		ShaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
		LocalRootArguments(pLocalRootArguments, localRootArgumentsSize)
	{}

	void CopyTo(void* pDest) const;

public:
	PointerWithSize ShaderIdentifier;
	PointerWithSize LocalRootArguments;
};

// Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
class ShaderTable : public GPUUploadBuffer
{
public:
	ShaderTable(ID3D12Device* pDevice, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName = nullptr);

	void PushBack(const ShaderRecord& shaderRecord);

	inline UINT GetShaderRecordSize() { return m_ShaderRecordSize; }

	// Pretty-print the shader records.
	void DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap);

private:
	ShaderTable() {}

private:
	UINT8* m_pMappedShaderRecords = nullptr;
	UINT m_ShaderRecordSize = 0;

	// Debug support
	WCHAR m_Name[512] = { 0, };
	std::vector<ShaderRecord> m_ShaderRecords;
};

void AllocateUAVBuffer(ID3D12Device* pDevice, UINT64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON, const WCHAR* resourceName = nullptr);

template<class T, size_t N>
void DefineExports(T* obj, LPCWSTR(&Exports)[N])
{
	for (UINT i = 0; i < N; i++)
	{
		obj->DefineExport(Exports[i]);
	}
}

template<class T, size_t N, size_t M>
void DefineExports(T* obj, LPCWSTR(&Exports)[N][M])
{
	for (UINT i = 0; i < N; i++)
		for (UINT j = 0; j < M; j++)
		{
			obj->DefineExport(Exports[i][j]);
		}
}


void AllocateUploadBuffer(ID3D12Device* pDevice, void* pData, UINT64 datasize, ID3D12Resource** ppResource, const WCHAR* resourceName = nullptr);

// Pretty-print a state object tree.
void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* pDesc);

// Returns bool whether the device supports DirectX Raytracing tier.
bool IsDirectXRaytracingSupported(IDXGIAdapter1* pAdapter);