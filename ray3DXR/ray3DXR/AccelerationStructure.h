#pragma once

#include "CommonDefine.h"

class Application;
class StructuredBuffer;

class AccelerationStructure
{
public:
	AccelerationStructure() = default;
	virtual ~AccelerationStructure() { Cleanup(); }

	bool Cleanup();

	inline UINT64 GetRequiredScratchSize() { return max(m_PrebuildInfo.ScratchDataSizeInBytes, m_PrebuildInfo.UpdateScratchDataSizeInBytes); }
	inline UINT64 GetRequiredResultDataSizeInBytes() { return m_PrebuildInfo.ResultDataMaxSizeInBytes; }
	inline ID3D12Resource* GetResource() { return m_pAccelerationStructure; }
	inline const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& GetPrebuildInfo() { return m_PrebuildInfo; }
	inline const WCHAR* GetName() { return m_szName; }
	inline UINT64 GetResourceSize() { return m_pAccelerationStructure->GetDesc().Width; }

	inline bool IsDirty() { return m_bIsDirty; }

	inline void SetDirty(bool bIsDirty) { m_bIsDirty = bIsDirty; }

protected:
	void AllocateResource(ID3D12Device5* pDevice);

protected:
	ID3D12Resource* m_pAccelerationStructure = nullptr;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO m_PrebuildInfo = {};
	WCHAR m_szName[MAX_PATH] = { 0, };

	bool m_bIsBuilt = false;
	bool m_bIsDirty = true;
	bool m_bUpdateOnBuild = false;
	bool m_bAllowUpdate = false;
};

class BottomLevelAccelerationStructureGeometry
{
public:
	BottomLevelAccelerationStructureGeometry() = default;
	BottomLevelAccelerationStructureGeometry(const WCHAR* pszNAME);
	~BottomLevelAccelerationStructureGeometry() = default;

	inline const WCHAR* GetName() { return m_szName; }
	inline std::vector<GeometryInstance>& GetGeometryInstances() { return m_GeometryInstances; }

	inline void SetName(const WCHAR* pszNAME) { wcsncpy_s(m_szName, MAX_PATH, pszNAME, wcslen(pszNAME)); }

public:
	WCHAR m_szName[MAX_PATH] = { 0, };
	std::vector<GeometryInstance> m_GeometryInstances;
	std::vector<GeometryBuffer> m_Geometries;
	std::vector<TextureBuffer> m_Textures;
	DXGI_FORMAT m_IndexFormat = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT m_VertexFormat = DXGI_FORMAT_UNKNOWN;
};

class BottomLevelAccelerationStructure final : public AccelerationStructure
{
public:
	BottomLevelAccelerationStructure() = default;
	~BottomLevelAccelerationStructure() = default;

	bool Initialize(ID3D12Device5* pDevice, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, BottomLevelAccelerationStructureGeometry* pBottomLevelASGeometry, bool bAllowUpdate = false, bool bUpdateOnBuild = false);
	bool Build(ID3D12GraphicsCommandList4* pCommandList, ID3D12Resource* pScratch, ID3D12DescriptorHeap* pDescriptorHeap, D3D12_GPU_VIRTUAL_ADDRESS baseGeometryTransformGPUAddress = 0);

	void UpdateGeometryDescsTransform(D3D12_GPU_VIRTUAL_ADDRESS baseGeometryTransformGPUAddress);

	inline UINT GetInstanceContributionToHitGroupIndex() { return m_InstanceContributionToHitGroupIndex; }
	inline const DirectX::XMMATRIX& GetTransform() { return m_Transform; }
	inline std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& GetGeometryDescs() { return m_GeometryDescs; }

	inline void SetInstanceContributionToHitGroupIndex(UINT index) { m_InstanceContributionToHitGroupIndex = index; }

private:
	void BuildGeometryDescs(BottomLevelAccelerationStructureGeometry* pBottomLevelASGeometry);
	void ComputePrebuildInfo(ID3D12Device5* pDevice);

private:
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_GeometryDescs;
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_CacheGeometryDescs[2];
	UINT CurrentID = 0;
	DirectX::XMMATRIX m_Transform;
	UINT m_InstanceContributionToHitGroupIndex = 0;
};

class TopLevelAccelerationStructure final : public AccelerationStructure
{
public:
	TopLevelAccelerationStructure() = default;
	~TopLevelAccelerationStructure() = default;

	bool Initialize(ID3D12Device5* pDevice, UINT numBottomLevelASInstanceDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool bAllowUpdate = false, bool bUpdateOnBuild = false, const WCHAR* pszResourceName = nullptr);
	bool Build(ID3D12GraphicsCommandList4* pCommandList, UINT numInstanceDescs, D3D12_GPU_VIRTUAL_ADDRESS instanceDescs, ID3D12Resource* pScratch, ID3D12DescriptorHeap* pDescriptorHeap, bool bUpdate = false);

private:
	void ComputePrebuildInfo(ID3D12Device5* pDevice, UINT numBottomLevelASInstanceDescs);
};

class AccelerationStructureManager
{
public:
	AccelerationStructureManager() = default;
	~AccelerationStructureManager() { Cleanup(); }

	bool Initialize(Application* pApp, UINT maxNumBottomLevelInstances);
	bool Cleanup();

	bool AddBottomLevelAS(ID3D12Device5* pDevice, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, BottomLevelAccelerationStructureGeometry* pBottomLevelASGeometry, bool bAllowUpdate = false, bool bPerformUpdateOnBuild = false);
	UINT AddBottomLevelASInstance(const WCHAR* pszBottomLevelASname, UINT instanceContributionToHitGroupIndex = UINT_MAX, DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity(), BYTE instanceMask = 1);
	bool InitializeTopLevelAS(ID3D12Device5* pDevice, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool bAllowUpdate = false, bool bPerformUpdateOnBuild = false, const WCHAR* pszResourceName = nullptr);

	bool Build(ID3D12GraphicsCommandList4* pCommandList, ID3D12DescriptorHeap* pDescriptorHeap, UINT frameIndex, bool bForceBuild = false);

	inline BottomLevelAccelerationStructure& GetBottomLevelAS(const WCHAR* pszNAME) { return m_BottomLevelASs[pszNAME]; }
	inline ID3D12Resource* GetTopLevelResource() { return m_TopLevelAS.GetResource(); }
	inline UINT64 GetASMemoryFootprint() { return m_ASMemoryFootprint; }
	UINT GetNumberOfBottomLevelASInstances();
	UINT GetMaxInstanceContributionToHitGroupIndex();

private:
	TopLevelAccelerationStructure m_TopLevelAS;
	std::map<std::wstring, BottomLevelAccelerationStructure> m_BottomLevelASs;

	StructuredBuffer* m_pBottomLevelASInstanceDescs = nullptr;
	UINT m_NumBottomLevelASInstances = 0;

	ID3D12Resource* m_pAccelerationStructureScratch = nullptr;
	UINT64 m_ScratchResourceSize = 0;

	UINT64 m_ASMemoryFootprint = 0;
};
