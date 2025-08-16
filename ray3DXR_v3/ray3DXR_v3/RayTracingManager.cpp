#include "pch.h"
#include <d3d12.h>
#include <d3dx12.h>
#include "typedef.h"
#include "D3DUtil.h"
#include "IndexCreator.h"
#include "D3D12ResourceManager.h"
#include "D3DResourceRecycleBin.h"
#include "SimpleConstantBufferPool.h"
#include "SingleDescriptorAllocator.h"
#include "ConstantBufferManager.h"
#include "ShaderManager.h"
#include "D3D12Renderer.h"
#include "ShaderTable.h"
#include "RayTracingManager.h"

const DWORD RAY_TRACING_SHADER_TYPE_NUM = 2;

const wchar_t* c_raygenShaderName = { L"MyRaygenShader_RadianceRay" };
const wchar_t* c_closestHitShaderName[RAY_TRACING_SHADER_TYPE_NUM] = { L"MyClosestHitShader_RadianceRay", L"MyClosestHitShader_ShadowRay" };
const wchar_t* c_missShaderName[RAY_TRACING_SHADER_TYPE_NUM] = { L"MyMissShader_RadianceRay", L"MyMissShader_ShadowRay" };
const wchar_t* c_anyHitShaderName[RAY_TRACING_SHADER_TYPE_NUM] = { L"MyAnyHitShader_RadianceRay", L"MyAnyHitShader_ShadowRay" };

// Hit groups.
const wchar_t* c_hitGroupName[RAY_TRACING_SHADER_TYPE_NUM] = { L"MyHitGroup_Triangle_RadianceRay", L"MyHitGroup_Triangle_ShadowRay" };

CRayTracingManager::CRayTracingManager()
{
}
BOOL CRayTracingManager::Initialize(CD3D12Renderer* pRenderer, DWORD dwWidth, DWORD dwHeight, DWORD dwMaxBlasCount)
{
	m_pRenderer = pRenderer;
	m_pD3DDevice = pRenderer->INL_GetD3DDevice();
	CShaderManager* pShaderManager = pRenderer->INL_GetShaderManager();

	m_dwMaxBlasCount = dwMaxBlasCount;
	m_ppCollectedBlasInstanceList = new BLAS_INSTANCE*[m_dwMaxBlasCount];
	memset(m_ppCollectedBlasInstanceList, 0, sizeof(BLAS_INSTANCE*) * m_dwMaxBlasCount);

	m_dwMaxShaderVisibleDescritporCount = DISPATCH_DESCRIPTOR_INDEX_COUNT + (LOCAL_ROOT_PARAM_DESCRIPTOR_COUNT * MAX_TRIGROUP_COUNT_PER_BLAS * dwMaxBlasCount);

	m_pIndexCreator = new CIndexCreator;
	m_pIndexCreator->Initialize(dwMaxBlasCount);

	m_pResourceBinTLAS = new CD3DResourceRecycleBin;
	m_pResourceBinTLAS->Initialize(m_pD3DDevice, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TopLevelAccelerationStructure");

	m_pResourceBinBLAS =  new CD3DResourceRecycleBin;
	m_pResourceBinBLAS->Initialize(m_pD3DDevice, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BottomLevelAccelerationStructure");

	m_pResourceBinScratchResource = new CD3DResourceRecycleBin;
	m_pResourceBinScratchResource->Initialize(m_pD3DDevice, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, L"ScratchResource");

	m_pResourceBinTLASInstanceDescList = new CD3DResourceRecycleBin;
	m_pResourceBinTLASInstanceDescList->Initialize(m_pD3DDevice, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, L"InstanceDesces");

	m_pResourceBinVBResource = new CD3DResourceRecycleBin;
	m_pResourceBinVBResource->Initialize(m_pD3DDevice, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, L"BLAS-VBResource");

	m_dwWidth = dwWidth;
	m_dwHeight = dwHeight;

	CreateDescriptorHeapCBV_SRV_UAV();
	CreateShaderVisibleHeap(m_dwMaxShaderVisibleDescritporCount);

	m_pRayShader = pShaderManager->CreateShaderDXC(L"Raytracing.hlsl", L"", L"lib_6_3", 0);

	CreateOutputDiffuseBuffer(m_dwWidth, m_dwHeight);
	CreateOutputDepthBuffer(m_dwWidth, m_dwHeight);

	CreateRootSignatures();
	CreateRaytracingPipelineStateObject();

	// Build Shader Tables
	BuildShaderTables();

	return TRUE;
}

void CRayTracingManager::CreateRaytracingPipelineStateObject()
{
	// 총 7개의 Subobject를 생성하여 RTPSO(Ray Tracing Pipeline State Object)를 구성
	// Subobject는 각각의 DXIL export(즉, 쉐이더 엔트리 포인트)에 기본 또는 명시적 방식으로 연결됨
		
	// 구성:
	// 1 - DXIL(DirectX Intermediate Language) library
	// 1 - Triangle hit group
	// 1 - Shader config (payload, attribute 크기)
	// 2 - Local root signature and association
	// 1 - Global root signature
	// 1 - Pipeline config (재귀 깊이 등)
	CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };


	// 1) DXIL 라이브러리 Subobject 생성
	// 셰이더는 서브오브젝트로 간주되지 않으므로 DXIL 라이브러리를 통해서 전달되어야 한다.
	// DXIL library
	// This contains the shaders and their entrypoints for the state object.
	// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
	CD3DX12_DXIL_LIBRARY_SUBOBJECT* pLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

	// Shader Bytecode 설정 (컴파일된 DXIL)
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(m_pRayShader->pCodeBuffer, m_pRayShader->dwCodeSize);
	pLib->SetDXILLibrary(&libdxil);

	//
	// DXIL 라이브러리에서 사용할 쉐이더 export들을 정의
	//
	pLib->DefineExport(c_raygenShaderName);

	// HitGroup에서 import할 수 있도록 export
	// 쉐이더 타입별(radiance/shadow)로 Closest Hit, Any Hit, Miss 쉐이더를 export
	for (DWORD i = 0; i < RAY_TRACING_SHADER_TYPE_NUM; i++)
	{	
		pLib->DefineExport(c_closestHitShaderName[i]);	// hit group에서 import할 수 있도록 export
		pLib->DefineExport(c_anyHitShaderName[i]);
		pLib->DefineExport(c_missShaderName[i]);
	}
	// 2) Triangle hit group
	// 히트 그룹 Subobject 생성
	// 히트 그룹은 Geometry에 레이가 교차했을 때 실행할 ClosestHit, AnyHit, Intersection 쉐이더를 정의
	for (DWORD i = 0; i < RAY_TRACING_SHADER_TYPE_NUM; i++)
	{
		CD3DX12_HIT_GROUP_SUBOBJECT* pHitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
		pHitGroup->SetClosestHitShaderImport(c_closestHitShaderName[i]);	// DXIL에서 export한 Closest Hit 쉐이더 연결
		pHitGroup->SetAnyHitShaderImport(c_anyHitShaderName[i]);			// DXIL에서 export한 AnyHitShader 쉐이더 연결
		pHitGroup->SetHitGroupExport(c_hitGroupName[i]);					// ShaderBindingTable에서 참조할 수 있도록 export - UpdateHitGroupShaderTable()에서 사용
		pHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);			// 삼각형 기반 히트 그룹 지정(삼각형이 아닌 수학적으로 정의되는 도형도 가능)
	}

	// 3) Shader config
	// Defines the maximum sizes in bytes for the ray payload and attribute structure.
	// Payload와 Attribute 구조의 최대 크기를 설정
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* pShaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = PAYLOAD_SIZE;
	UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
	pShaderConfig->Config(payloadSize, attributeSize);

	// 4,5) Local root signature and shader association
	// Local Root Signature 및 연결 설정 (명시적 연결 사용)
	// Shader Table에서 각 쉐이더가 고유한 인자를 받을 수 있도록 해줌
	//
	// Raytracing Pipeline State Object에 Local Root Signature 서브오브젝트를 추가
	//
	CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT* pLocalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	pLocalRootSignature->SetRootSignature(m_pRaytracingLocalRootSignature);		// LOCAL_ROOT_SIGNATURE_SUBOBJECT로 앞서 생성한 Local RootSignature(Closest Shader/AnyHit Shader에서 사용)를 설정.

	// Hit Group에 Local Root Signature를 연결하기 위한 Association 서브오브젝트 생성
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* pRootSignatureAssociation = raytracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();

	// 앞서 생성한 Local Root Signature가 다른 서브오브젝트에 연결될 수 있도록 설정.
	pRootSignatureAssociation->SetSubobjectToAssociate(*pLocalRootSignature);

	// 해당 Local Root Signature를 HitGroup에 연결.
	pRootSignatureAssociation->AddExports(c_hitGroupName);
	// pRootSignatureAssociation->AddExports(c_hitGroupName[0]) - "MyHitGroup_Triangle_RadianceRay" 
	// pRootSignatureAssociation->AddExports(c_hitGroupName[1]) - "MyHitGroup_Triangle_ShadowRay"


	// 6) Global root signature
	// Global Root Signature Subobject 생성
	// DispatchRays() 호출 중 모든 쉐이더가 볼 수 있는 루트 시그니처
	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* pGlobalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	pGlobalRootSignature->SetRootSignature(m_pRaytracingGlobalRootSignature);

	// 7) Pipeline config
	// TraceRay() 함수의 최대 재귀 깊이를 설정
	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* pPipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	// PERFOMANCE TIP: Set max recursion depth as low as needed 
	// as drivers may apply optimization strategies for low recursion depths. 
	UINT maxRecursionDepth = MAX_RECURSION_DEPTH; // ~ primary rays only. 
	pPipelineConfig->Config(maxRecursionDepth);


	// Create the state object.
	const D3D12_STATE_OBJECT_DESC* pRaytracingPipeline = raytracingPipeline;
	if (FAILED(m_pD3DDevice->CreateStateObject(pRaytracingPipeline, IID_PPV_ARGS(&m_pDXRStateObject))))
	{
		__debugbreak();
	}
	m_pDXRStateObject->SetName(L"CRayTracingManager::m_pDXRStateObject");
}

void CRayTracingManager::BuildShaderTables()
{
	// Get shader identifiers.
	ID3D12StateObjectProperties* pStateObjectProperties = nullptr;
	m_pDXRStateObject->QueryInterface(IID_PPV_ARGS(&pStateObjectProperties));

	void* pRayGenShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(c_raygenShaderName);

	// raygen shader table
	ShaderRecord rayGenShaderRecord = ShaderRecord(pRayGenShaderIdentifier, m_ShaderIdentifierSize, nullptr, 0);
	m_pRayGenShaderTable = new CShaderTable;
	m_pRayGenShaderTable->Initiailze(m_pD3DDevice, m_ShaderIdentifierSize, L"RayGenShaderTable");
	m_pRayGenShaderTable->CommitResource(1);
	m_pRayGenShaderTable->InsertShaderRecord(&rayGenShaderRecord);

	// Miss shader table
	m_pMissShaderTable = new CShaderTable;
	m_pMissShaderTable->Initiailze(m_pD3DDevice, m_ShaderIdentifierSize, L"MissShaderTable");
	m_pMissShaderTable->CommitResource(RAY_TRACING_SHADER_TYPE_NUM);
	for (DWORD i = 0; i < RAY_TRACING_SHADER_TYPE_NUM; i++)
	{
		void* pMissShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(c_missShaderName[i]);
		ShaderRecord missShaderRecord = ShaderRecord(pMissShaderIdentifier, m_ShaderIdentifierSize);
		m_pMissShaderTable->InsertShaderRecord(&missShaderRecord);
	}
	m_MissShaderTableStrideInBytes = m_pMissShaderTable->GetShaderRecordSize();

	// hitgroup Shader Table
	m_pHitGroupShaderTable = new CShaderTable;
	m_pHitGroupShaderTable->Initiailze(m_pD3DDevice, m_ShaderIdentifierSize + sizeof(ROOT_ARG), L"HitGroupShaderTable");
	m_HitGroupShaderRecordSize = m_pHitGroupShaderTable->GetShaderRecordSize();

	//
	// HitGroupShader Table내용을 채워넣는 작업은 UpdateHitGroupShaderTable()에서 수행. 어차피 프레임마다 수행되어야 한다.
	//

	if (pStateObjectProperties)
	{
		pStateObjectProperties->Release();
		pStateObjectProperties = nullptr;
	}
}
void CRayTracingManager::UpdateHitGroupShaderTable(DWORD dwShaderRecordCount)
{
	//
	// wait필요
	//

	// ShaderRecord in HitGroup Table
	// |                 0               |                 1               | .... |                 N-1             |        
	// | [ShaderIdntifier-RootArguments] | [ShaderIdntifier-RootArguments] | .... | [ShaderIdntifier-RootArguments] |

	// Get shader identifiers.
	ID3D12StateObjectProperties* pStateObjectProperties = nullptr;
	m_pDXRStateObject->QueryInterface(IID_PPV_ARGS(&pStateObjectProperties));

	// hitgroup Shader Table
	void* pHitGroupShaderIdentifier[RAY_TRACING_SHADER_TYPE_NUM] = {};
	for (DWORD i = 0; i < RAY_TRACING_SHADER_TYPE_NUM; i++)
	{
		pHitGroupShaderIdentifier[i] = pStateObjectProperties->GetShaderIdentifier(c_hitGroupName[i]);
	}
	m_pHitGroupShaderTable->CommitResource(dwShaderRecordCount);

	SORT_LINK* pCur = m_pBlasInstanceLinkHead;
	DWORD dwShaderRecordIndex = 0;
	while (pCur)
	{
		BLAS_INSTANCE* pBlasInstance = (BLAS_INSTANCE*)pCur->pItem;

		// pBlasInstance->ShaderRecordIndex는 HitGroupShaderTable에서의 ShaderRecord 시작 인덱스.
		// 이 값은 TLAS빌드 시에 D3D12_RAYTRACING_INSTANCE_DESC::InstanceContributionToHitGroupIndex에 대입한다.
		pBlasInstance->ShaderRecordIndex = dwShaderRecordIndex;

		for (DWORD i = 0; i < pBlasInstance->dwTriGroupCount; i++)
		{
			for (DWORD j = 0; j < RAY_TRACING_SHADER_TYPE_NUM; j++)
			{
				ShaderRecord record = ShaderRecord(pHitGroupShaderIdentifier[j], m_ShaderIdentifierSize, &pBlasInstance->pRootArg[i], sizeof(ROOT_ARG));
				m_pHitGroupShaderTable->InsertShaderRecord(&record);
				dwShaderRecordIndex++;
			}
		}
		pCur = pCur->pNext;
	}
	m_HitGroupShaderTableStrideInBytes = m_pHitGroupShaderTable->GetShaderRecordSize();
	m_dwHitGroupShaderRecordNum = m_pHitGroupShaderTable->GetShaderRecordNum();

	if (pStateObjectProperties)
	{
		pStateObjectProperties->Release();
		pStateObjectProperties = nullptr;
	}
}
void CRayTracingManager::UpdateBLASTransform(BLAS_INSTANCE* pBlasInstance, const XMMATRIX* pMatWorld)
{
	pBlasInstance->matTransform = *pMatWorld;
	m_dwUpdateAccelerationStructureFlags |= UPDATE_ACCELERATION_STRCTURE_TYPE_TLAS;
}
void CRayTracingManager::SetUpdateBLAS(BLAS_INSTANCE* pBlasInstance)
{
	pBlasInstance->bMustUpdatBlas = TRUE;
	m_dwUpdateAccelerationStructureFlags |= UPDATE_ACCELERATION_STRCTURE_TYPE_BLAS;
	m_dwUpdateAccelerationStructureFlags |= UPDATE_ACCELERATION_STRCTURE_TYPE_TLAS;	// BLAS를 감싸는 바운딩매시의 모양이 바뀔 수 있으므로 TLAS를 업데이트 한다.
}
void CRayTracingManager::UpdateManagedResource()
{
	ULONGLONG CurTick = GetTickCount64();
	m_pResourceBinTLAS->Update(CurTick);
	m_pResourceBinBLAS->Update(CurTick);
	m_pResourceBinScratchResource->Update(CurTick);
	m_pResourceBinTLASInstanceDescList->Update(CurTick);
	m_pResourceBinVBResource->Update(CurTick);
}
BOOL CRayTracingManager::UpdateAccelerationStructure(ID3D12GraphicsCommandList6* pCommandList)
{
	BOOL bResult = FALSE;

	// TLAS빌드를 위해 유효한 BLAS들을 수집 및 HitGroupShaderTable에 몇 개의 ShaderRecord가 들어갈지 카운트
	
	DWORD dwBlasInstanceCount = 0;

	// BLAS에서 필요한 ShaderRecord개수를 카운트.
	// 아직 BLAS자료구조가 생성되지 않았거나 업데이트가 필요한 경우, 생성 및 업데이트
	DWORD	dwRequiredShaderRecordCount = 0;
	SORT_LINK* pCur = m_pBlasInstanceLinkHead;
	while (pCur)
	{
		BLAS_INSTANCE* pBlasInstance = (BLAS_INSTANCE*)pCur->pItem;
		if (dwBlasInstanceCount >= m_dwMaxBlasCount)
			__debugbreak();

		if (!pBlasInstance->pBLAS)
		{
			BuildBLAS(pCommandList, pBlasInstance);
		}
		dwRequiredShaderRecordCount += (pBlasInstance->dwTriGroupCount * RAY_TRACING_SHADER_TYPE_NUM);
		m_ppCollectedBlasInstanceList[dwBlasInstanceCount] = pBlasInstance;
		dwBlasInstanceCount++;

		pCur = pCur->pNext;
	}

	// UDPATE BLAS
	if (m_dwUpdateAccelerationStructureFlags & UPDATE_ACCELERATION_STRCTURE_TYPE_BLAS)
	{
		pCur = m_pBlasInstanceLinkHead;
		while (pCur)
		{
			BLAS_INSTANCE* pBlasInstance = (BLAS_INSTANCE*)pCur->pItem;
			if (pBlasInstance->bMustUpdatBlas)
			{
				pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(pBlasInstance->pBLAS));
				
				UpdateBLAS(pCommandList, pBlasInstance->pBLAS, pBlasInstance->pGeomDescList, pBlasInstance->dwTriGroupCount);
				
				pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(pBlasInstance->pBLAS));
				pBlasInstance->bMustUpdatBlas = FALSE;

			}
			else
			{
				int a = 0;
			}
			pCur = pCur->pNext;
		}
		m_dwUpdateAccelerationStructureFlags &= (~UPDATE_ACCELERATION_STRCTURE_TYPE_BLAS);
	}
	if (m_dwUpdateAccelerationStructureFlags & UPDATE_ACCELERATION_STRCTURE_TYPE_HIT_GROUP_SHADER_TABLE)
	{
		// HitGroupShaderTable갱신과 함께 BLAS별로 ShderReocordIndex를 설정한다.
		UpdateHitGroupShaderTable(dwRequiredShaderRecordCount);
		m_dwUpdateAccelerationStructureFlags &= (~UPDATE_ACCELERATION_STRCTURE_TYPE_HIT_GROUP_SHADER_TABLE);
	}
	// TLAS빌드
	if (m_dwUpdateAccelerationStructureFlags & UPDATE_ACCELERATION_STRCTURE_TYPE_TLAS)
	{
		if (m_pTLAS)
		{
			m_pResourceBinTLAS->Free(m_pTLAS, MAX_PENDING_FRAME_COUNT);
			m_pTLAS = nullptr;
		}

		if (m_pBLASInstanceDescResouce)
		{
			m_pResourceBinTLASInstanceDescList->Free(m_pBLASInstanceDescResouce, MAX_PENDING_FRAME_COUNT);
			m_pBLASInstanceDescResouce = nullptr;
		}
		// TLAS빌드를 위한 인스턴스 리소스 할당
		m_pBLASInstanceDescResouce = m_pResourceBinTLASInstanceDescList->Alloc(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * dwBlasInstanceCount);

		m_pTLAS = BuildTLAS(pCommandList, m_pBLASInstanceDescResouce, m_ppCollectedBlasInstanceList, dwBlasInstanceCount, FALSE, 0);
		m_dwUpdateAccelerationStructureFlags &= (~UPDATE_ACCELERATION_STRCTURE_TYPE_TLAS);
	}
	bResult = TRUE;
	return bResult;
}
BLAS_INSTANCE* CRayTracingManager::AllocBLAS(ID3D12Resource* pVertexBuffer, UINT VertexSize, DWORD dwVertexCount, const BLAS_BUILD_TRIGROUP_INFO* pTriGroupInfoList, DWORD dwTriGroupInfoCount, BOOL bAllowUpdate)
{
	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	CSingleDescriptorAllocator* pSingleDescriptorAllocator = m_pRenderer->INL_GetSingleDescriptorAllocator();
	BLAS_INSTANCE* pBlasInstance = nullptr;

	if (m_dwBlasCount >= m_dwMaxBlasCount)
		goto lb_return;

	//
	// VB한개, IB여러개
	//
	// 추후에 ID3D12CommandList포인터와 UINT CurContextIndex를 받아서 중첩렌더링을 처리할 수 있도록 수정한다.
	//

	if (dwTriGroupInfoCount > MAX_TRIGROUP_COUNT_PER_BLAS)
		__debugbreak();

	DWORD dwIndex = m_pIndexCreator->Alloc();
	if (-1 == dwIndex)
		goto lb_return;

	DWORD dwBlasInstanceMemSize = (DWORD)sizeof(BLAS_INSTANCE) - sizeof(ROOT_ARG) + (DWORD)sizeof(ROOT_ARG) * dwTriGroupInfoCount;

	pBlasInstance = (BLAS_INSTANCE*)malloc(dwBlasInstanceMemSize);
	memset(pBlasInstance, 0, dwBlasInstanceMemSize);
	pBlasInstance->dwID = dwIndex;
	pBlasInstance->matTransform = XMMatrixIdentity();
	pBlasInstance->dwVertexCount = dwVertexCount;

	D3D12_GPU_VIRTUAL_ADDRESS VB_GPU_Ptr = {};
	if (bAllowUpdate)
	{	
		// 버텍스 목록 변형 가능
		pBlasInstance->pVBResourceUpdated = m_pResourceBinVBResource->Alloc(dwVertexCount * sizeof(BasicVertex));
		VB_GPU_Ptr = pBlasInstance->pVBResourceUpdated->GetGPUVirtualAddress();

		D3D12_RESOURCE_DESC descBuf = pBlasInstance->pVBResourceUpdated->GetDesc();
		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		UAVDesc.Buffer.StructureByteStride = sizeof(BasicVertex);
		UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
		UAVDesc.Buffer.NumElements = (UINT)(descBuf.Width / sizeof(BasicVertex));
		UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		UAVDesc.Buffer.CounterOffsetInBytes = 0;
		
		D3D12_CPU_DESCRIPTOR_HANDLE	uavOutVB = {};
		pSingleDescriptorAllocator->AllocDescriptorHandle(&uavOutVB);
		m_pD3DDevice->CreateUnorderedAccessView(pBlasInstance->pVBResourceUpdated, nullptr, &UAVDesc, uavOutVB);
		pBlasInstance->uavVBResourceUpdated = uavOutVB;
	}
	else
	{
		// 버텍스 목록 변형 불가
		VB_GPU_Ptr = pVertexBuffer->GetGPUVirtualAddress();
	}
	D3D12_RAYTRACING_GEOMETRY_DESC* pGeomDescList = pBlasInstance->pGeomDescList;

	for (DWORD i = 0; i < dwTriGroupInfoCount; i++)
	{
		D3D12_GPU_VIRTUAL_ADDRESS IB_GPU_Ptr = pTriGroupInfoList[i].pIB->GetGPUVirtualAddress();

		pGeomDescList[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		pGeomDescList[i].Triangles.IndexBuffer = IB_GPU_Ptr;
		pGeomDescList[i].Triangles.IndexCount = pTriGroupInfoList[i].dwIndexNum;
		pGeomDescList[i].Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		pGeomDescList[i].Triangles.Transform3x4 = 0;
		pGeomDescList[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		pGeomDescList[i].Triangles.VertexCount = dwVertexCount;
		pGeomDescList[i].Triangles.VertexBuffer.StartAddress = VB_GPU_Ptr;
		pGeomDescList[i].Triangles.VertexBuffer.StrideInBytes = VertexSize;
		// Mark the geometry as opaque. 
		// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
		// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
		if (pTriGroupInfoList[i].bNotOpaque)
		{
			pGeomDescList[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
		}
		else
		{
			pGeomDescList[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		}
	}


	// set LocalRoot Params
	{
		UINT DescriptorIndex = DISPATCH_DESCRIPTOR_INDEX_COUNT + (LOCAL_ROOT_PARAM_DESCRIPTOR_COUNT * MAX_TRIGROUP_COUNT_PER_BLAS) * pBlasInstance->dwID;
		CD3DX12_CPU_DESCRIPTOR_HANDLE	srvCpu(m_pShaderVisibleDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), DescriptorIndex, m_DescriptorSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE	srvGpu(m_pShaderVisibleDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), DescriptorIndex, m_DescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		for (DWORD i = 0; i < dwTriGroupInfoCount; i++)
		{
			pBlasInstance->pRootArg[i].cb.mtl = pTriGroupInfoList[i].mtl;

			// Create ShaderResource from Vertex Buffer
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = dwVertexCount;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.Buffer.StructureByteStride = VertexSize;

			pD3DDevice->CreateShaderResourceView(pVertexBuffer, &srvDesc, srvCpu);
			pBlasInstance->pRootArg[i].srvVB = srvGpu;
			srvCpu.Offset(1, m_DescriptorSize);
			srvGpu.Offset(1, m_DescriptorSize);

			// Create ShaderResource from Index Buffer			
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = (pTriGroupInfoList[i].dwIndexNum * 4) / 4;	// compute shader에서 4bytes 단위로 읽어야 하므로...
			srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			srvDesc.Buffer.StructureByteStride = 0;

			m_pD3DDevice->CreateShaderResourceView(pTriGroupInfoList[i].pIB, &srvDesc, srvCpu);
			pBlasInstance->pRootArg[i].srvIB = srvGpu;
			srvCpu.Offset(1, m_DescriptorSize);
			srvGpu.Offset(1, m_DescriptorSize);

			// diffuse texture
			if (pTriGroupInfoList[i].pDiffuseTexHandle)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE srvTexSrc = pTriGroupInfoList[i].pDiffuseTexHandle->srv;
				if (srvTexSrc.ptr)
				{
					pD3DDevice->CopyDescriptorsSimple(1, srvCpu, srvTexSrc, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
			}
			pBlasInstance->pRootArg[i].srvTexDiffuse = srvGpu;
			srvCpu.Offset(1, m_DescriptorSize);
			srvGpu.Offset(1, m_DescriptorSize);

			// normal texture
			if (pTriGroupInfoList[i].pNormalTexHandle)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE srvTexSrc = pTriGroupInfoList[i].pNormalTexHandle->srv;
				if (srvTexSrc.ptr)
				{
					pD3DDevice->CopyDescriptorsSimple(1, srvCpu, srvTexSrc, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
			}
			pBlasInstance->pRootArg[i].srvTexNormal = srvGpu;
			srvCpu.Offset(1, m_DescriptorSize);
			srvGpu.Offset(1, m_DescriptorSize);

		}
	}
	pBlasInstance->Link.pItem = pBlasInstance;
	pBlasInstance->Link.pNext = nullptr;
	pBlasInstance->Link.pPrv = nullptr;
	pBlasInstance->pBLAS = nullptr;	// 아직은 D3DResource로서의 BLAS는 생성 전이다.
	pBlasInstance->dwTriGroupCount = dwTriGroupInfoCount;
	pBlasInstance->bAllowUpdate = bAllowUpdate;

	LinkToLinkedListFIFO(&m_pBlasInstanceLinkHead, &m_pBlasInstanceLinkTail, &pBlasInstance->Link);
	m_dwBlasCount++;
	m_dwUpdateAccelerationStructureFlags = UPDATE_ACCELERATION_STRCTURE_TYPE_HIT_GROUP_SHADER_TABLE | UPDATE_ACCELERATION_STRCTURE_TYPE_TLAS;
lb_return:
	return pBlasInstance;
}
void CRayTracingManager::FreeBLAS(BLAS_INSTANCE* pBlasInstance)
{
	CSingleDescriptorAllocator* pSingleDescriptorAllocator = m_pRenderer->INL_GetSingleDescriptorAllocator();

	DWORD dwIndex = pBlasInstance->dwID;
	m_pIndexCreator->Free(dwIndex);

	pBlasInstance->dwID = -1;

	if (pBlasInstance->pBLAS)
	{
		m_pResourceBinBLAS->Free(pBlasInstance->pBLAS, MAX_PENDING_FRAME_COUNT);
		pBlasInstance->pBLAS = nullptr;
	}
	if (pBlasInstance->uavVBResourceUpdated.ptr)
	{
		pSingleDescriptorAllocator->FreeDescriptorHandle(pBlasInstance->uavVBResourceUpdated);
		pBlasInstance->uavVBResourceUpdated.ptr = 0;
	}
	if (pBlasInstance->pVBResourceUpdated)
	{
		m_pResourceBinVBResource->Free(pBlasInstance->pVBResourceUpdated, MAX_PENDING_FRAME_COUNT);
		pBlasInstance->pVBResourceUpdated = nullptr;
	}
	UnLinkFromLinkedList(&m_pBlasInstanceLinkHead, &m_pBlasInstanceLinkTail, &pBlasInstance->Link);
	m_dwBlasCount--;
	
	free(pBlasInstance);
	
	m_dwUpdateAccelerationStructureFlags = UPDATE_ACCELERATION_STRCTURE_TYPE_HIT_GROUP_SHADER_TABLE | UPDATE_ACCELERATION_STRCTURE_TYPE_TLAS;

}
void CRayTracingManager::FreeAllBLAS()
{
	while (m_pBlasInstanceLinkHead)
	{
		BLAS_INSTANCE* pBlasInstance = (BLAS_INSTANCE*)m_pBlasInstanceLinkHead->pItem;
		FreeBLAS(pBlasInstance);
	}
}
BOOL CRayTracingManager::BuildBLAS(ID3D12GraphicsCommandList6* pCommandList, BLAS_INSTANCE* pBlasInstance)
{
	// BLAS를 새로 빌드, 또는 업데이트
	BOOL bResult = FALSE;

	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();

	if (pBlasInstance->dwTriGroupCount > MAX_TRIGROUP_COUNT_PER_BLAS)
		__debugbreak();

	if (pBlasInstance->pBLAS)
	{
		m_pResourceBinBLAS->Free(pBlasInstance->pBLAS, MAX_PENDING_FRAME_COUNT);
		pBlasInstance->pBLAS = nullptr;
	}
	// Build BLAS
	
	// Get required sizes for an acceleration structure.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	if (pBlasInstance->bAllowUpdate)
	{
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	}
	else
	{
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
	}
	inputs.NumDescs = pBlasInstance->dwTriGroupCount;
	inputs.pGeometryDescs = pBlasInstance->pGeomDescList;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	pD3DDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	ID3D12Resource*	pScratchResource = m_pResourceBinScratchResource->Alloc(info.ScratchDataSizeInBytes);

	D3D12_GPU_VIRTUAL_ADDRESS pScratchGPUAddress = pScratchResource->GetGPUVirtualAddress();

	if (!pScratchGPUAddress)
		__debugbreak();
	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn뭪 need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
		
	//D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	//if (FAILED(CreateUAVBuffer(m_pD3DDevice, info.ResultDataMaxSizeInBytes, &pBLAS, initialResourceState, L"BottomLevelAccelerationStructure")))
	//	__debugbreak();
	//pBlasInstance->pBLAS = m_pResourceBinBLAS->Alloc(info.ScratchDataSizeInBytes);
	pBlasInstance->pBLAS = m_pResourceBinBLAS->Alloc(info.ResultDataMaxSizeInBytes);

	// Bottom Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.ScratchAccelerationStructureData = pScratchGPUAddress;
	asDesc.DestAccelerationStructureData = pBlasInstance->pBLAS->GetGPUVirtualAddress();

		

	pCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(pBlasInstance->pBLAS));

	if (pScratchResource)
	{
		m_pResourceBinScratchResource->Free(pScratchResource, MAX_PENDING_FRAME_COUNT);
		pScratchResource = nullptr;
	}
	bResult = TRUE;
	return bResult;
}
BOOL CRayTracingManager::UpdateBLAS(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pBLAS, const D3D12_RAYTRACING_GEOMETRY_DESC* pGeomDescList, DWORD dwGeomCount)
{
	// BLAS를 새로 빌드, 또는 업데이트
	BOOL bResult = FALSE;

	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();

	// Update BLAS

	// Get required sizes for an acceleration structure.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

	inputs.NumDescs = dwGeomCount;
	inputs.pGeometryDescs = pGeomDescList;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	m_pD3DDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	ID3D12Resource*	pScratchResource = m_pResourceBinScratchResource->Alloc(info.ScratchDataSizeInBytes);

	D3D12_GPU_VIRTUAL_ADDRESS pScratchGPUAddress = pScratchResource->GetGPUVirtualAddress();
	if (!pScratchGPUAddress)
		__debugbreak();

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn뭪 need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.

	D3D12_RESOURCE_DESC		descBlas = pBLAS->GetDesc();
	if (descBlas.Width < info.ResultDataMaxSizeInBytes)
		__debugbreak();

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn't need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.

	// Bottom Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.SourceAccelerationStructureData = pBLAS->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = pScratchGPUAddress;
	asDesc.DestAccelerationStructureData = pBLAS->GetGPUVirtualAddress();
	pCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	//pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pBLAS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));


	// 배리어는 바깥에서 넣어준다.
	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	//pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(pBLAS));

	if (pScratchResource)
	{
		m_pResourceBinScratchResource->Free(pScratchResource, MAX_PENDING_FRAME_COUNT);
		pScratchResource = nullptr;
	}
	bResult = TRUE;
	return bResult;
}
ID3D12Resource* CRayTracingManager::BuildTLAS(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pInstanceDescResource, BLAS_INSTANCE** ppInstanceList, DWORD dwBlasInstanceNum, BOOL bAllowUpdate, UINT CurContextIndex)
{
	ID3D12Resource* pTLASResource = nullptr;

	// First, get the size of the TLAS buffers and create them
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	if (bAllowUpdate)
	{
		//inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
		inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	}
	else
	{
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
	}
	inputs.NumDescs = dwBlasInstanceNum;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
	m_pD3DDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);


	ID3D12Resource*	pScratchResource = m_pResourceBinScratchResource->Alloc(info.ScratchDataSizeInBytes);

	D3D12_GPU_VIRTUAL_ADDRESS pScratchGPUAddress = pScratchResource->GetGPUVirtualAddress();

	pTLASResource = m_pResourceBinTLAS->Alloc(info.ResultDataMaxSizeInBytes);

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn't need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.

	//
	// Create an instance desc for the bottom-level acceleration structure.
	//ID3D12Resource* pInstanceDescResource = nullptr;
	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDescList = nullptr;

	CD3DX12_RANGE readRange(0, 0);
	pInstanceDescResource->Map(0, &readRange, (void**)&pInstanceDescList);

	DWORD	dwTLAS_ElementCount = 0;
	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDescEntry = pInstanceDescList;
	for (DWORD i = 0; i < dwBlasInstanceNum; i++)
	{
		const BLAS_INSTANCE*	pInstanceSrc = ppInstanceList[i];
		XMMATRIX matTranspose = XMMatrixTranspose(pInstanceSrc->matTransform);
		memcpy(pInstanceDescEntry->Transform, &matTranspose, sizeof(pInstanceDescEntry->Transform));

		pInstanceDescEntry->InstanceID = pInstanceSrc->dwID; // This value will be exposed to the shader via InstanceID()
		pInstanceDescEntry->InstanceContributionToHitGroupIndex = pInstanceSrc->ShaderRecordIndex;
		pInstanceDescEntry->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

		memcpy(pInstanceDescEntry->Transform, &matTranspose, sizeof(pInstanceDescEntry->Transform));
		pInstanceDescEntry->AccelerationStructure = pInstanceSrc->pBLAS->GetGPUVirtualAddress();
		pInstanceDescEntry->InstanceMask = 0xFF;
		pInstanceDescEntry++;
		dwTLAS_ElementCount++;
	}

	// Unmap
	pInstanceDescResource->Unmap(0, nullptr);

	// Create the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	//asDesc.Inputs.NumDescs = dwBlasInstanceNum;
	asDesc.Inputs.NumDescs = dwTLAS_ElementCount;
	asDesc.Inputs.InstanceDescs = pInstanceDescResource->GetGPUVirtualAddress();
	asDesc.DestAccelerationStructureData = pTLASResource->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = pScratchGPUAddress;

	pCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = pTLASResource;
	pCommandList->ResourceBarrier(1, &uavBarrier);

	if (pScratchResource)
	{
		m_pResourceBinScratchResource->Free(pScratchResource, MAX_PENDING_FRAME_COUNT);
		pScratchResource = nullptr;
	}

	return pTLASResource;
}


void CRayTracingManager::CleanupShaderTables()
{
	if (m_pRayGenShaderTable)
	{
		delete m_pRayGenShaderTable;
		m_pRayGenShaderTable = nullptr;
	}
	if (m_pMissShaderTable)
	{
		delete m_pMissShaderTable;
		m_pMissShaderTable = nullptr;
	}
	if (m_pHitGroupShaderTable)
	{
		delete m_pHitGroupShaderTable;
		m_pHitGroupShaderTable = nullptr;
	}
}
void CRayTracingManager::CreateDescriptorHeapCBV_SRV_UAV()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = COMMON_DESCRIPTOR_COUNT;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(m_pD3DDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_pCommonDescriptorHeap))))
		__debugbreak();

	m_pCommonDescriptorHeap->SetName(L"CD3D12Renderer::m_pCommonDescriptorHeap");

	m_DescriptorSize = m_pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
void CRayTracingManager::CleanupDescriptorHeapForCBV_SRV_UAV()
{
	if (m_pCommonDescriptorHeap)
	{
		m_pCommonDescriptorHeap->Release();
		m_pCommonDescriptorHeap = nullptr;
	}
}
void CRayTracingManager::CreateShaderVisibleHeap(DWORD dwMaxDescriptorCount)
{
	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
	HeapDesc.NumDescriptors = dwMaxDescriptorCount;
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	if (FAILED(m_pD3DDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&m_pShaderVisibleDescriptorHeap))))
	{
		__debugbreak();
	}
}
void CRayTracingManager::CleanupDispatchHeap()
{
	if (m_pShaderVisibleDescriptorHeap)
	{
		m_pShaderVisibleDescriptorHeap->Release();
		m_pShaderVisibleDescriptorHeap = nullptr;
	}
}
void CRayTracingManager::UpdateWindowSize(DWORD dwWidth, DWORD dwHeight)
{
	CleanupOutputDiffuseBuffer();
	CleanupOutputDepthBuffer();

	m_dwWidth = dwWidth;
	m_dwHeight = dwHeight;
	CreateOutputDiffuseBuffer(m_dwWidth, m_dwHeight);
	CreateOutputDepthBuffer(m_dwWidth, m_dwHeight);
}
void CRayTracingManager::DoRaytracing(ID3D12GraphicsCommandList6* pCommandList, D3D12_CPU_DESCRIPTOR_HANDLE srvSkyCubeMap)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE	dispatchHeapHandleCPU(m_pShaderVisibleDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_CPU_DESCRIPTOR_HANDLE	cbvHandle = {};
	CSimpleConstantBufferPool* pConstantBufferPool = m_pRenderer->GetConstantBufferPool(CONSTANT_BUFFER_TYPE_RAY_TRACING);
	CB_CONTAINER* pCB = pConstantBufferPool->Alloc();
	if (!pCB)
	{
		__debugbreak();
	}

	CONSTANT_BUFFER_RAY_TRACING* pConstBuffer = (CONSTANT_BUFFER_RAY_TRACING*)pCB->pSystemMemAddr;
	m_pRenderer->FillRayTraceConstant(pConstBuffer);
	pConstBuffer->MaxRadianceRayRecursionDepth = MAX_RADIANCE_RECURSION_DEPTH;
	pConstBuffer->MaxShadowRayRecursionDepth = MAX_SHADOW_RECURSION_DEPTH;
	
	
	// Lights
	RT_LIGHT LightList[MAX_RT_LIGHT_COUNT] = {};
	XMVECTOR v = { 0.25f, -1.0f, 0.5f, 0.0f };
	XMStoreFloat3(&LightList[0].Pos_Dir, XMVector3Normalize(v));
	LightList[0].Rs = 1000.0f;
	LightList[0].Color = { 0.5f, 1.0f, 1.0f };
	LightList[0].Type = RT_LIGHT_TYPE_DIRECTIONAL;
	pConstBuffer->LightCount = 1;
	memcpy(pConstBuffer->LightList, LightList, pConstBuffer->LightCount * sizeof(RT_LIGHT));

	// (0) CBV - RayTracing
	m_pD3DDevice->CopyDescriptorsSimple(1, dispatchHeapHandleCPU, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dispatchHeapHandleCPU.Offset(1, m_DescriptorSize);

	// (1) UAV - output diffuse
	CD3DX12_CPU_DESCRIPTOR_HANDLE	uavDiffuse(m_pCommonDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), COMMON_DESCRIPTOR_INDEX_OUTPUT_DIFFUSE_UAV, m_DescriptorSize);
	m_pD3DDevice->CopyDescriptorsSimple(1, dispatchHeapHandleCPU, uavDiffuse, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dispatchHeapHandleCPU.Offset(1, m_DescriptorSize);

	// (2) UAV - output depth
	CD3DX12_CPU_DESCRIPTOR_HANDLE	uavDepth(m_pCommonDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), COMMON_DESCRIPTOR_INDEX_OUTPUT_DEPTH_UAV, m_DescriptorSize);
	m_pD3DDevice->CopyDescriptorsSimple(1, dispatchHeapHandleCPU, uavDepth, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dispatchHeapHandleCPU.Offset(1, m_DescriptorSize);

	// (3) SRV - sky cubemap
	if (srvSkyCubeMap.ptr)
	{
		m_pD3DDevice->CopyDescriptorsSimple(1, dispatchHeapHandleCPU, srvSkyCubeMap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	dispatchHeapHandleCPU.Offset(1, m_DescriptorSize);


	CD3DX12_RESOURCE_BARRIER rcBarrier[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_pOutputDiffuse, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pOutputDepth, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
	};
	pCommandList->ResourceBarrier((UINT)_countof(rcBarrier), rcBarrier);

	pCommandList->SetComputeRootSignature(m_pRaytracingGlobalRootSignature);

	// Bind the heaps, acceleration structure and dispatch rays.    
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	ID3D12DescriptorHeap*	ppHeaps[] = { m_pShaderVisibleDescriptorHeap };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE	dispatchHeapHandleGPU(m_pShaderVisibleDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	pCommandList->SetComputeRootDescriptorTable(0, dispatchHeapHandleGPU);
	pCommandList->SetComputeRootShaderResourceView(1, m_pTLAS->GetGPUVirtualAddress());

	// hit group shader table
	ID3D12Resource*	pHitGroupShaderTableResource = m_pHitGroupShaderTable->GetResource();
	dispatchDesc.HitGroupTable.StartAddress = pHitGroupShaderTableResource->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = m_pHitGroupShaderTable->GetHitGroupShaderTableSize();
	dispatchDesc.HitGroupTable.StrideInBytes = m_HitGroupShaderTableStrideInBytes;

	// miss shader table
	ID3D12Resource* pMissShaderTableResource = m_pMissShaderTable->GetResource();
	dispatchDesc.MissShaderTable.StartAddress = pMissShaderTableResource->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = m_pMissShaderTable->GetHitGroupShaderTableSize();
	dispatchDesc.MissShaderTable.StrideInBytes = m_MissShaderTableStrideInBytes;

	// raygen shader table
	ID3D12Resource* pRayGenShaderTableResource = m_pRayGenShaderTable->GetResource();
	dispatchDesc.RayGenerationShaderRecord.StartAddress = pRayGenShaderTableResource->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_pRayGenShaderTable->GetShaderRecordSize();

	dispatchDesc.Width = m_dwWidth;
	dispatchDesc.Height = m_dwHeight;
	dispatchDesc.Depth = 1;

	pCommandList->SetPipelineState1(m_pDXRStateObject);
	pCommandList->DispatchRays(&dispatchDesc);

	CD3DX12_RESOURCE_BARRIER rcBarrierInv[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_pOutputDiffuse, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pOutputDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
	};
	pCommandList->ResourceBarrier((UINT)_countof(rcBarrierInv), rcBarrierInv);

}

void CRayTracingManager::CreateRootSignatures()
{
	// Global Root Signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.

	// root param 0
	// output-diffuse(uav) | output-depth(uav)

	// root param 1
	// Acceleration Sturecture

	CD3DX12_DESCRIPTOR_RANGE globalRanges[3];
	// b0 : global constant | u0 : out-diffuse | u1 : out-depth | t10 : sky cubemap tex
	globalRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);	
	globalRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);
	globalRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 10);

	// b0 : RaytracingCBV | u0 : u0-diffuse | u1 : out-depth | t0 : AccelerationStructure
	CD3DX12_ROOT_PARAMETER GlobalRootParameters[2];
	GlobalRootParameters[0].InitAsDescriptorTable(_countof(globalRanges), globalRanges, D3D12_SHADER_VISIBILITY_ALL);
	GlobalRootParameters[1].InitAsShaderResourceView(0);	// Acceleration Structure

	// 샘플러
	D3D12_STATIC_SAMPLER_DESC samplers[4] = {};
	SetSamplerDesc_Wrap(samplers + 0, 0);	// Wrap Linear
	SetSamplerDesc_Clamp(samplers + 1, 1);	// Clamp Linear
	SetSamplerDesc_Wrap(samplers + 2, 2);	// Wrap Point
	samplers[2].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	SetSamplerDesc_Mirror(samplers + 3, 3);	// Mirror Linear
	samplers[3].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

	for (DWORD i = 0; i < (DWORD)_countof(samplers); i++)
	{
		samplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(GlobalRootParameters), GlobalRootParameters, (DWORD)_countof(samplers), samplers);
	SerializeAndCreateRaytracingRootSignature(m_pD3DDevice, &globalRootSignatureDesc, &m_pRaytracingGlobalRootSignature);

	// Local Root Signature

	// space1
	// b0 : CBV Per TriGroup , t0 : VertexBuffer , t1 : IndexBuffer, t2 : Tex Diffuse, t3: Tex Normal
	CD3DX12_DESCRIPTOR_RANGE localRanges[1] = {};
	localRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 1);

	CD3DX12_ROOT_PARAMETER LocalRootParameters[2];
	LocalRootParameters[0].InitAsConstants(SizeOfInUint32(CONSTANT_BUFFER_RT_TRIGROUP), 0, 1);	// b0 : CBV Per TriGroup
	LocalRootParameters[1].InitAsDescriptorTable(_countof(localRanges), localRanges);			// t0 : VertexBuffer , t1 : IndexBuffer, t2 : Tex Diffuse

	CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(LocalRootParameters), LocalRootParameters);
	localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	SerializeAndCreateRaytracingRootSignature(m_pD3DDevice, &localRootSignatureDesc, &m_pRaytracingLocalRootSignature);
}
BOOL CRayTracingManager::CreateOutputDiffuseBuffer(UINT Width, UINT Height)
{
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Width = Width;
	texDesc.Height = Height;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	texDesc.DepthOrArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	if (FAILED(m_pD3DDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_pOutputDiffuse))))
	{
		__debugbreak();
	}
	m_pOutputDiffuse->SetName(L"CRayTracingManager::m_pOutputDiffuse");

	// Create UAV

	CD3DX12_CPU_DESCRIPTOR_HANDLE	uavHandle(m_pCommonDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), COMMON_DESCRIPTOR_INDEX_OUTPUT_DIFFUSE_UAV, m_DescriptorSize);
	m_pD3DDevice->CreateUnorderedAccessView(m_pOutputDiffuse, nullptr, nullptr, uavHandle);

	return TRUE;
}
void CRayTracingManager::CleanupOutputDiffuseBuffer()
{
	if (m_pOutputDiffuse)
	{
		m_pOutputDiffuse->Release();
		m_pOutputDiffuse = nullptr;
	}
}

BOOL CRayTracingManager::CreateOutputDepthBuffer(UINT Width, UINT Height)
{

	// Create Output Buffer, Texture, SRV
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	texDesc.Width = Width;
	texDesc.Height = Height;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	//texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	texDesc.DepthOrArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;


	if (FAILED(m_pD3DDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_pOutputDepth))))
	{
		__debugbreak();
	}
	m_pOutputDepth->SetName(L"CRayTracingManager::m_pOutputDepth");

	// Create UAV
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	UAVDesc.Buffer.StructureByteStride = sizeof(float);
	UAVDesc.Buffer.NumElements = Width * Height;
	UAVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	CD3DX12_CPU_DESCRIPTOR_HANDLE	uavHandle(m_pCommonDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), COMMON_DESCRIPTOR_INDEX_OUTPUT_DEPTH_UAV, m_DescriptorSize);
	m_pD3DDevice->CreateUnorderedAccessView(m_pOutputDepth, nullptr, &UAVDesc, uavHandle);

	return TRUE;
}
void CRayTracingManager::CleanupOutputDepthBuffer()
{
	if (m_pOutputDepth)
	{
		m_pOutputDepth->Release();
		m_pOutputDepth = nullptr;
	}
}




void CRayTracingManager::Cleanup()
{
	CShaderManager* pShaderManager = m_pRenderer->INL_GetShaderManager();

	CleanupOutputDiffuseBuffer();
	CleanupOutputDepthBuffer();

	CleanupShaderTables();

	// Cleanup DXRStateObject
	if (m_pDXRStateObject)
	{
		m_pDXRStateObject->Release();
		m_pDXRStateObject = nullptr;
	}
	// Cleanup RootSignature
	if (m_pRaytracingGlobalRootSignature)
	{
		m_pRaytracingGlobalRootSignature->Release();
		m_pRaytracingGlobalRootSignature = nullptr;
	}
	if (m_pRaytracingLocalRootSignature)
	{
		m_pRaytracingLocalRootSignature->Release();
		m_pRaytracingLocalRootSignature = nullptr;
	}
	if (m_pRayShader)
	{
		pShaderManager->ReleaseShader(m_pRayShader);
		m_pRayShader = nullptr;
	}
	if (m_ppCollectedBlasInstanceList)
	{
		delete[] m_ppCollectedBlasInstanceList;
		m_ppCollectedBlasInstanceList = nullptr;
	}
	FreeAllBLAS();

	if (m_pTLAS)
	{
		m_pResourceBinTLAS->Free(m_pTLAS, 1);
		m_pTLAS = nullptr;
	}
	if (m_pBLASInstanceDescResouce)
	{
		m_pResourceBinTLASInstanceDescList->Free(m_pBLASInstanceDescResouce, 1);
		m_pBLASInstanceDescResouce = nullptr;
	}
	if (m_pResourceBinBLAS)
	{
		delete m_pResourceBinBLAS;
		m_pResourceBinBLAS = nullptr;
	}
	if (m_pResourceBinTLAS)
	{
		delete m_pResourceBinTLAS;
		m_pResourceBinTLAS = nullptr;
	}
	if (m_pResourceBinScratchResource)
	{
		delete m_pResourceBinScratchResource;
		m_pResourceBinScratchResource = nullptr;
	}
	if (m_pResourceBinTLASInstanceDescList)
	{
		delete m_pResourceBinTLASInstanceDescList;
		m_pResourceBinTLASInstanceDescList = nullptr;
	}
	if (m_pResourceBinVBResource)
	{
		delete m_pResourceBinVBResource;
		m_pResourceBinVBResource = nullptr;
	}

	CleanupDescriptorHeapForCBV_SRV_UAV();
	CleanupDispatchHeap();

	if (m_pIndexCreator)
	{
		delete m_pIndexCreator;
		m_pIndexCreator = nullptr;
	}
}
CRayTracingManager::~CRayTracingManager()
{
	Cleanup();
}