#pragma once

class ConstantBuffer
{
public:
	ConstantBuffer() = default;
	~ConstantBuffer() { Cleanup(); }

	bool Initialize(ID3D12Device5* pDevice, UINT dataSize, const void* pDATA);
	bool Cleanup();

	bool Upload();

	inline ID3D12Resource* GetResource() { return m_pResource; }
	inline void* GetDataMem() { return m_pMemData; }
	inline UINT GetBufferSize() { return m_BufferSize; }
	inline UINT GetDataSize() { return m_DataSize; }

private:
	ID3D12Resource* m_pResource = nullptr;
	BYTE* m_pMappedResource = nullptr;
	void* m_pMemData = nullptr;

	UINT m_BufferSize = 0;
	UINT m_DataSize = 0;
};

class StructuredBuffer
{
public:
	StructuredBuffer() = default;
	~StructuredBuffer() { Cleanup(); }

	bool Initialize(ID3D12Device5* pDevice, UINT sizePerData, UINT numData, const void* pDATA);
	bool Cleanup();

	bool Upload();

	inline ID3D12Resource* GetResource() { return m_pResource; }
	inline void* GetDataMem() { return m_pMemData; }
	inline UINT GetBufferSize() { return m_BufferSize; }
	inline UINT GetNumData() { return m_NumData; }

private:
	ID3D12Resource* m_pResource = nullptr;
	void* m_pMappedResource = nullptr;
	void* m_pMemData = nullptr;

	UINT m_BufferSize = 0;
	UINT m_NumData = 0;
};
