#include "framework.h"
#include "Application.h"
#include "GPUResource.h"
#include "AccelerationStructure.h"

bool AccelerationStructure::Cleanup()
{
	if (m_pAccelerationStructure)
	{
		m_pAccelerationStructure->Release();
		m_pAccelerationStructure = nullptr;
	}
	m_BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	m_PrebuildInfo = { 0, };
	m_bIsBuilt = false;
	m_bIsDirty = true;
	m_bUpdateOnBuild = false;
	m_bAllowUpdate = false;

	return true;
}

void AccelerationStructure::AllocateResource(ID3D12Device5* pDevice)
{
	_ASSERT(pDevice);
	_ASSERT(m_PrebuildInfo.ResultDataMaxSizeInBytes > 0);

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_PrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	HRESULT hr = pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&m_pAccelerationStructure));
	if (FAILED(hr))
	{
		__debugbreak();
	}
	m_pAccelerationStructure->SetName(m_szName);
}

BottomLevelAccelerationStructureGeometry::BottomLevelAccelerationStructureGeometry(const WCHAR* pszNAME)
{
	_ASSERT(pszNAME);
	wcsncpy_s(m_szName, MAX_PATH, pszNAME, wcslen(pszNAME));
}

bool BottomLevelAccelerationStructure::Initialize(ID3D12Device5* pDevice, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, BottomLevelAccelerationStructureGeometry* pBottomLevelASGeometry, bool bAllowUpdate, bool bUpdateOnBuild)
{
	_ASSERT(pDevice);
	_ASSERT(pBottomLevelASGeometry);

	m_bAllowUpdate = bAllowUpdate;
	m_bUpdateOnBuild = bUpdateOnBuild;

	m_BuildFlags = buildFlags;
	if (pBottomLevelASGeometry->GetName())
	{
		wcsncpy_s(m_szName, MAX_PATH, pBottomLevelASGeometry->GetName(), wcslen(pBottomLevelASGeometry->GetName()));
	}

	if (bAllowUpdate)
	{
		m_BuildFlags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	}

	BuildGeometryDescs(pBottomLevelASGeometry);
	ComputePrebuildInfo(pDevice);
	AllocateResource(pDevice);

	m_bIsDirty = true;
	m_bIsBuilt = false;

	return true;
}

bool BottomLevelAccelerationStructure::Build(ID3D12GraphicsCommandList4* pCommandList, ID3D12Resource* pScratch, ID3D12DescriptorHeap* pDescriptorHeap, D3D12_GPU_VIRTUAL_ADDRESS baseGeometryTransformGPUAddress)
{
	_ASSERT(pCommandList);
	_ASSERT(pScratch);
	_ASSERT(pDescriptorHeap);
	_ASSERT(m_pAccelerationStructure);
	
	if (m_PrebuildInfo.ScratchDataSizeInBytes > pScratch->GetDesc().Width)
	{
		//__debugbreak();
		//L"Insufficient scratch buffer size provided!";
		return false;
	}

	if (baseGeometryTransformGPUAddress > 0)
	{
		UpdateGeometryDescsTransform(baseGeometryTransformGPUAddress);
	}

	CurrentID = (CurrentID + 1) % 2;
	m_CacheGeometryDescs[CurrentID].clear();
	m_CacheGeometryDescs[CurrentID].resize(m_GeometryDescs.size());
	std::copy(m_GeometryDescs.begin(), m_GeometryDescs.end(), m_CacheGeometryDescs[CurrentID].begin());

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottomLevelInputs.Flags = m_BuildFlags;
	if (m_bIsBuilt && m_bAllowUpdate && m_bUpdateOnBuild)
	{
		bottomLevelInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		bottomLevelBuildDesc.SourceAccelerationStructureData = m_pAccelerationStructure->GetGPUVirtualAddress();
	}
	bottomLevelInputs.NumDescs = (UINT)m_CacheGeometryDescs[CurrentID].size();
	bottomLevelInputs.pGeometryDescs = m_CacheGeometryDescs[CurrentID].data();

	bottomLevelBuildDesc.ScratchAccelerationStructureData = pScratch->GetGPUVirtualAddress();
	bottomLevelBuildDesc.DestAccelerationStructureData = m_pAccelerationStructure->GetGPUVirtualAddress();

	pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);
	pCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

	m_bIsDirty = false;
	m_bIsBuilt = true;

	return true;
}

void BottomLevelAccelerationStructure::UpdateGeometryDescsTransform(D3D12_GPU_VIRTUAL_ADDRESS baseGeometryTransformGPUAddress)
{
	struct alignas(16) AlignedGeometryTransform3x4
	{
		float Transform3x4[12];
	};

	for (SIZE_T i = 0, size = m_GeometryDescs.size(); i < size; ++i)
	{
		D3D12_RAYTRACING_GEOMETRY_DESC& geometryDesc = m_GeometryDescs[i];
		geometryDesc.Triangles.Transform3x4 = baseGeometryTransformGPUAddress + i * sizeof(AlignedGeometryTransform3x4);
	}
}

void BottomLevelAccelerationStructure::BuildGeometryDescs(BottomLevelAccelerationStructureGeometry* pBottomLevelASGeometry)
{
	_ASSERT(pBottomLevelASGeometry);

	D3D12_RAYTRACING_GEOMETRY_DESC geometryDescBase = {};
	geometryDescBase.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDescBase.Triangles.IndexFormat = pBottomLevelASGeometry->m_IndexFormat;
	geometryDescBase.Triangles.VertexFormat = pBottomLevelASGeometry->m_VertexFormat;

	m_GeometryDescs.reserve(pBottomLevelASGeometry->m_GeometryInstances.size());
	for (SIZE_T i = 0, size = pBottomLevelASGeometry->m_GeometryInstances.size(); i < size; ++i)
	{
		GeometryInstance& geometry = pBottomLevelASGeometry->m_GeometryInstances[i];
		geometryDescBase.Flags = geometry.GeometryFlags;
		geometryDescBase.Triangles.IndexBuffer = geometry.IB.IndexBuffer;
		geometryDescBase.Triangles.IndexCount = geometry.IB.Count;
		geometryDescBase.Triangles.VertexBuffer = geometry.VB.VertexBuffer;
		geometryDescBase.Triangles.VertexCount = geometry.VB.Count;
		geometryDescBase.Triangles.Transform3x4 = geometry.Transform;

		m_GeometryDescs.push_back(geometryDescBase);
	}
}

void BottomLevelAccelerationStructure::ComputePrebuildInfo(ID3D12Device5* pDevice)
{
	_ASSERT(pDevice);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottomLevelInputs.Flags = m_BuildFlags;
	bottomLevelInputs.NumDescs = (UINT)m_GeometryDescs.size();
	bottomLevelInputs.pGeometryDescs = m_GeometryDescs.data();

	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &m_PrebuildInfo);
	if (m_PrebuildInfo.ResultDataMaxSizeInBytes <= 0)
	{
		__debugbreak();
	}
}

bool TopLevelAccelerationStructure::Initialize(ID3D12Device5* pDevice, UINT numBottomLevelASInstanceDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool bAllowUpdate, bool bUpdateOnBuild, const WCHAR* pszResourceName)
{
	_ASSERT(pDevice);
	_ASSERT(pszResourceName);
	_ASSERT(numBottomLevelASInstanceDescs > 0);

	m_bAllowUpdate = bAllowUpdate;
	m_bUpdateOnBuild = bUpdateOnBuild;
	m_BuildFlags = buildFlags;
	wcsncpy_s(m_szName, MAX_PATH, pszResourceName, wcslen(pszResourceName));

	if (bAllowUpdate)
	{
		m_BuildFlags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	}

	ComputePrebuildInfo(pDevice, numBottomLevelASInstanceDescs);
	AllocateResource(pDevice);

	m_bIsDirty = true;
	m_bIsBuilt = false;

	return true;
}

bool TopLevelAccelerationStructure::Build(ID3D12GraphicsCommandList4* pCommandList, UINT numInstanceDescs, D3D12_GPU_VIRTUAL_ADDRESS instanceDescs, ID3D12Resource* pScratch, ID3D12DescriptorHeap* pDescriptorHeap, bool bUpdate)
{
	_ASSERT(pCommandList);
	_ASSERT(numInstanceDescs > 0);
	_ASSERT(instanceDescs != 0);
	_ASSERT(pScratch);
	_ASSERT(pDescriptorHeap);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = m_BuildFlags;
	if (m_bIsBuilt && m_bAllowUpdate && m_bUpdateOnBuild)
	{
		topLevelInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	}
	topLevelInputs.NumDescs = numInstanceDescs;

	topLevelBuildDesc.ScratchAccelerationStructureData = pScratch->GetGPUVirtualAddress();
	topLevelBuildDesc.DestAccelerationStructureData = m_pAccelerationStructure->GetGPUVirtualAddress();
	topLevelInputs.InstanceDescs = instanceDescs;

	pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);
	pCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
	m_bIsDirty = false;
	m_bIsBuilt = true;

	return true;
}

void TopLevelAccelerationStructure::ComputePrebuildInfo(ID3D12Device5* pDevice, UINT numBottomLevelASInstanceDescs)
{
	_ASSERT(pDevice);
	_ASSERT(numBottomLevelASInstanceDescs > 0);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = m_BuildFlags;
	topLevelInputs.NumDescs = numBottomLevelASInstanceDescs;

	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &m_PrebuildInfo);
	if (m_PrebuildInfo.ResultDataMaxSizeInBytes <= 0)
	{
		__debugbreak();
	}
}

bool AccelerationStructureManager::Initialize(Application* pApp, UINT maxNumBottomLevelInstances)
{
	_ASSERT(pApp);
	_ASSERT(maxNumBottomLevelInstances > 0);

	m_pBottomLevelASInstanceDescs = new StructuredBuffer;
	return m_pBottomLevelASInstanceDescs->Initialize(pApp, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), maxNumBottomLevelInstances, nullptr);
}

bool AccelerationStructureManager::Cleanup()
{
	for (auto iter = m_BottomLevelASs.begin(), endIter = m_BottomLevelASs.end(); iter != endIter; ++iter)
	{
		iter->second.Cleanup();
	}
	m_BottomLevelASs.clear();

	if (m_pBottomLevelASInstanceDescs)
	{
		delete m_pBottomLevelASInstanceDescs;
		m_pBottomLevelASInstanceDescs = nullptr;
	}

	if (m_pAccelerationStructureScratch)
	{
		m_pAccelerationStructureScratch->Release();
		m_pAccelerationStructureScratch = nullptr;
	}

	m_NumBottomLevelASInstances = 0;
	m_ScratchResourceSize = 0;
	m_ASMemoryFootprint = 0;

	return true;
}

bool AccelerationStructureManager::AddBottomLevelAS(ID3D12Device5* pDevice, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, BottomLevelAccelerationStructureGeometry* pBottomLevelASGeometry, bool bAllowUpdate, bool bPerformUpdateOnBuild)
{
	_ASSERT(pDevice);
	_ASSERT(pBottomLevelASGeometry);

	if (m_BottomLevelASs.find(pBottomLevelASGeometry->GetName()) != m_BottomLevelASs.end())
	{
		//__debugbreak();
		//L"A bottom level acceleration structure with that name already exists."
		return false;
	}

	BottomLevelAccelerationStructure& bottomLevelAS = m_BottomLevelASs[pBottomLevelASGeometry->GetName()];
	if (!bottomLevelAS.Initialize(pDevice, buildFlags, pBottomLevelASGeometry, bAllowUpdate))
	{
		return false;
	}

	m_ASMemoryFootprint += bottomLevelAS.GetRequiredResultDataSizeInBytes();
	m_ScratchResourceSize = max(bottomLevelAS.GetRequiredScratchSize(), m_ScratchResourceSize);

	m_BottomLevelASs[bottomLevelAS.GetName()] = bottomLevelAS;

	return true;
}

UINT AccelerationStructureManager::AddBottomLevelASInstance(const WCHAR* pszBottomLevelASName, UINT instanceContributionToHitGroupIndex, DirectX::XMMATRIX transform, BYTE instanceMask)
{
	_ASSERT(pszBottomLevelASName);

	if (m_NumBottomLevelASInstances >= m_pBottomLevelASInstanceDescs->GetNumData())
	{
		//L"Not enough instance desc buffer size."
		return -1;
	}
	   
	UINT instanceIndex = m_NumBottomLevelASInstances++;
	BottomLevelAccelerationStructure& bottomLevelAS = m_BottomLevelASs[pszBottomLevelASName];

	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDescs = (D3D12_RAYTRACING_INSTANCE_DESC*)m_pBottomLevelASInstanceDescs->GetDataMem();
	D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc = pInstanceDescs[instanceIndex];
	instanceDesc.InstanceMask = instanceMask;
	instanceDesc.InstanceContributionToHitGroupIndex = (instanceContributionToHitGroupIndex != UINT_MAX ? instanceContributionToHitGroupIndex : bottomLevelAS.GetInstanceContributionToHitGroupIndex());
	instanceDesc.AccelerationStructure = bottomLevelAS.GetResource()->GetGPUVirtualAddress();
	DirectX::XMStoreFloat3x4((DirectX::XMFLOAT3X4*)instanceDesc.Transform, transform);

	return instanceIndex;
}

bool AccelerationStructureManager::InitializeTopLevelAS(ID3D12Device5* pDevice, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool bAllowUpdate, bool bPerformUpdateOnBuild, const WCHAR* pszResourceName)
{
	_ASSERT(pDevice);
	_ASSERT(!m_pAccelerationStructureScratch);

	if (!m_TopLevelAS.Initialize(pDevice, GetNumberOfBottomLevelASInstances(), buildFlags, bAllowUpdate, bPerformUpdateOnBuild, pszResourceName))
	{
		return false;
	}

	m_ASMemoryFootprint += m_TopLevelAS.GetRequiredResultDataSizeInBytes();
	m_ScratchResourceSize = max(m_TopLevelAS.GetRequiredScratchSize(), m_ScratchResourceSize);

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_ScratchResourceSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	HRESULT hr = pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_pAccelerationStructureScratch));
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

bool AccelerationStructureManager::Build(ID3D12GraphicsCommandList4* pCommandList, ID3D12DescriptorHeap* pDescriptorHeap, UINT frameIndex, bool bForceBuild)
{
	_ASSERT(pCommandList);
	_ASSERT(pDescriptorHeap);

	m_pBottomLevelASInstanceDescs->Upload();

	// Build all bottom-level AS.
	for (auto iter = m_BottomLevelASs.begin(), endIter = m_BottomLevelASs.end(); iter != endIter; ++iter)
	{
		BottomLevelAccelerationStructure& bottomLevelAS = iter->second;
		if (bForceBuild || bottomLevelAS.IsDirty())
		{
			D3D12_GPU_VIRTUAL_ADDRESS baseGeometryTransformGpuAddress = 0;
			if (!bottomLevelAS.Build(pCommandList, m_pAccelerationStructureScratch, pDescriptorHeap, baseGeometryTransformGpuAddress))
			{
				return false;
			}

			pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAS.GetResource()));
		}
	}

	// Build the top-level AS.
	{
		bool bPerformUpdate = false; // Always rebuild top-level Acceleration Structure.
		D3D12_GPU_VIRTUAL_ADDRESS instanceDescs = m_pBottomLevelASInstanceDescs->GetResource()->GetGPUVirtualAddress();
		if (!m_TopLevelAS.Build(pCommandList, GetNumberOfBottomLevelASInstances(), instanceDescs, m_pAccelerationStructureScratch, pDescriptorHeap, bPerformUpdate))
		{
			return false;
		}

		pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_TopLevelAS.GetResource()));
	}

	return true;
}

UINT AccelerationStructureManager::GetNumberOfBottomLevelASInstances()
{
	_ASSERT(m_pBottomLevelASInstanceDescs);
	return m_pBottomLevelASInstanceDescs->GetNumData();
}

UINT AccelerationStructureManager::GetMaxInstanceContributionToHitGroupIndex()
{
	UINT maxInstanceContributionToHitGroupIndex = 0;
	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDatas = (D3D12_RAYTRACING_INSTANCE_DESC*)m_pBottomLevelASInstanceDescs->GetDataMem();
	for (UINT i = 0; i < m_NumBottomLevelASInstances; i++)
	{
		D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc = pInstanceDatas[i];
		maxInstanceContributionToHitGroupIndex = max(maxInstanceContributionToHitGroupIndex, instanceDesc.InstanceContributionToHitGroupIndex);
	}
	return maxInstanceContributionToHitGroupIndex;
}
