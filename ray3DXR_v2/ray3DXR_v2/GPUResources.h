#pragma once

class GPUUploadBuffer
{
public:
    inline ID3D12Resource* GetResource() { return m_pResource; }

protected:
    GPUUploadBuffer() = default;
    virtual ~GPUUploadBuffer();

    void Allocate(ID3D12Device* pDevice, UINT bufferSize, LPCWSTR pszRresourceName = nullptr);

    BYTE* MapCPUWriteOnly();

protected:
    ID3D12Resource* m_pResource = nullptr;
};


class ShaderRecord final
{
public:
	class PointerWithSize final
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
class ShaderTable final : public GPUUploadBuffer
{
public:
	ShaderTable(ID3D12Device* pDevice, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR pszResourceName = nullptr);

	void PushBack(const ShaderRecord& shaderRecord);

	// Pretty-print the shader records.
	void DebugPrint(std::unordered_map<void*, std::wstring>& shaderIdToStringMap);

	inline UINT GetShaderRecordSize() { return m_ShaderRecordSize; }

private:
	ShaderTable() = default;
	~ShaderTable() = default;

private:
	BYTE* m_pMappedShaderRecords = nullptr;
	UINT m_ShaderRecordSize = 0;

	// Debug support
	WCHAR m_szName[512] = { 0, };
	std::vector<ShaderRecord> m_ShaderRecords;
};
