#pragma once

struct AccerlationStructureBuffer
{
	ID3D12Resource2* pScratch;
	ID3D12Resource2* pAccelerationStructure;
	ID3D12Resource2* pInstanceDesc;
	UINT64 ResultDatMaxSizeInBytes;
};

class AccelerationStructure
{
public:
	AccelerationStructure() = default;
	virtual ~AccelerationStructure() { CleanupD3DResources(); }

	void CleanupD3DResources();

	inline void SetDirty(bool bIsDirty) { m_bIsDirty = bIsDirty; }
	inline bool IsDirty() { return m_bIsDirty; }

	inline ID3D12Resource2* GetResource() { return m_pAccelerationStructure; }
	inline UINT64 GetResourceSize() { return m_pAccelerationStructure->GetDesc().Width; }
	inline UINT64 GetRequiredScratchSize() { return max(m_PrebuildInfo.ScratchDataSizeInBytes, m_PrebuildInfo.UpdateScratchDataSizeInBytes); }
	inline UINT64 GetRequiredResultDataSizeInBytes() { return m_PrebuildInfo.ResultDataMaxSizeInBytes; }
	inline const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* GetPrebuildInfo() { return &m_PrebuildInfo; }
	inline const WCHAR* GetName() { return m_szName; }

protected:
	bool AllocateResource(ID3D12Device5* pDevice);

protected:
	WCHAR m_szName[MAX_PATH] = { 0, };

	ID3D12Resource2* m_pAccelerationStructure = nullptr;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO m_PrebuildInfo = {};
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

	bool m_bIsBuilt = false;
	bool m_bIsDirty = true;
	bool m_bUpdateOnBuild = false;
	bool m_bAllowUpdate = false;
};

class BottomLevelAccelerationStructureGeometry final
{
public:
	BottomLevelAccelerationStructureGeometry() = default;
	~BottomLevelAccelerationStructureGeometry() = default;

	void SetName(const WCHAR* pszName);

	inline WCHAR* GetName() { return szName; }

public:
	WCHAR szName[MAX_PATH] = { 0, };
	//std::vector<GeometryInstance> m_GeometryInstances;
	//std::vector<D3DGeometry> m_Geometries;
	//std::vector<D3DTexture> m_Textures;
	UINT m_NumTriangles = 0;
	DXGI_FORMAT m_IndexFormat = DXGI_FORMAT_UNKNOWN;
	UINT m_IBStrideInBytes = 0;
	DXGI_FORMAT m_VertexFormat = DXGI_FORMAT_UNKNOWN;
	UINT m_VBStrideInBytes = 0;
};

class BottomLevelAccelerationStructure final : public AccelerationStructure
{
public:
	BottomLevelAccelerationStructure() = default;
	~BottomLevelAccelerationStructure() = default;

	bool Initialize(ID3D12Device5* pDevice, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, BottomLevelAccelerationStructureGeometry* pBottomLevelASGeometry, bool bAllowUpdate = false, bool bUpdateOnBuild = false);
	bool Build(ID3D12GraphicsCommandList4* pCommandList, ID3D12Resource* pScratch, ID3D12DescriptorHeap* pDescriptorHeap, D3D12_GPU_VIRTUAL_ADDRESS baseGeometryTransformGPUAddress = 0);

	bool UpdateGeometryDescsTransform(D3D12_GPU_VIRTUAL_ADDRESS baseGeometryTransformGPUAddress);

	inline void SetInstanceContributionToHitGroupIndex(UINT index) { m_InstanceContributionToHitGroupIndex = index; }

	inline UINT GetInstanceContributionToHitGroupIndex() { return m_InstanceContributionToHitGroupIndex; }
	inline const DirectX::XMMATRIX* GetTransform() { return &m_Transform; }
	inline std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>* GetGeometryDescs() { return &m_GeometryDescs; }

private:
	bool BuildGeometryDescs(BottomLevelAccelerationStructureGeometry* pBottomLevelASGeometry);
	bool ComputePrebuildInfo(ID3D12Device5* pDevice);

private:
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_GeometryDescs;
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_CacheGeometryDescs[3];
	UINT m_CurrentID = 0;
	UINT m_InstanceContributionToHitGroupIndex = 0;
	DirectX::XMMATRIX m_Transform;
};
