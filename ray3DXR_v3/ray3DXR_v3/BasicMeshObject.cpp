#include "pch.h"
#include "typedef.h"
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include "D3DUtil.h"
#include "D3D12ResourceManager.h"
#include "ShaderManager.h"
#include "SimpleConstantBufferPool.h"
#include "SingleDescriptorAllocator.h"
#include "DescriptorPool.h"
#include "D3D12Renderer.h"
#include "RayTracingManager.h"
#include "BasicMeshObject.h"

using namespace DirectX;
SHADER_HANDLE* CBasicMeshObject::m_pVS = nullptr;
SHADER_HANDLE* CBasicMeshObject::m_pVS_Deform = nullptr;
SHADER_HANDLE* CBasicMeshObject::m_pCS_Deform = nullptr;
SHADER_HANDLE* CBasicMeshObject::m_pPS = nullptr;
ID3D12RootSignature* CBasicMeshObject::m_pRootSignature = nullptr;
ID3D12PipelineState* CBasicMeshObject::m_pPipelineState = nullptr;
ID3D12PipelineState* CBasicMeshObject::m_pPipelineState_Deform = nullptr;
ID3D12PipelineState* CBasicMeshObject::m_pPipelineState_CS_Deform = nullptr;
DWORD CBasicMeshObject::m_dwInitRefCount = 0;

CBasicMeshObject::CBasicMeshObject()
{
}

BOOL CBasicMeshObject::Initialize(CD3D12Renderer* pRenderer)
{
	m_pRenderer = pRenderer;

	BOOL bResult = InitCommonResources();
	return bResult;
}
BOOL CBasicMeshObject::InitCommonResources()
{
	if (m_dwInitRefCount)
		goto lb_true;

	InitRootSinagture();
	InitPipelineState();

lb_true:
	m_dwInitRefCount++;
	return m_dwInitRefCount;
}
BOOL CBasicMeshObject::InitRootSinagture()
{
	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	ID3DBlob* pSignature = nullptr;
	ID3DBlob* pError = nullptr;

	// Object - CBV - RootParam(0)
	// {
	//   TriGrup 0 - SRV[0] - RootParam(1) - Draw()
	//   TriGrup 1 - SRV[1] - RootParam(1) - Draw()
	//   TriGrup 2 - SRV[2] - RootParam(1) - Draw()
	//   TriGrup 3 - SRV[3] - RootParam(1) - Draw()
	//   TriGrup 4 - SRV[4] - RootParam(1) - Draw()
	//   TriGrup 5 - SRV[5] - RootParam(1) - Draw()
	// }

	CD3DX12_DESCRIPTOR_RANGE rangesPerObj[3] = {};
	rangesPerObj[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);	// b0 : Constant Buffer View per Object
	rangesPerObj[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);	// t3 : VertexBuffer
	rangesPerObj[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 8);	// u8 : VertexBuffer ouput

	CD3DX12_DESCRIPTOR_RANGE rangesPerTriGroup[1] = {};
	rangesPerTriGroup[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);	// t0 : Shader Resource View(Tex) per Tri-Group
	
	CD3DX12_ROOT_PARAMETER rootParameters[2] = {};
	rootParameters[0].InitAsDescriptorTable(_countof(rangesPerObj), rangesPerObj, D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[1].InitAsDescriptorTable(_countof(rangesPerTriGroup), rangesPerTriGroup, D3D12_SHADER_VISIBILITY_ALL);


	// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	SetDefaultSamplerDesc(&sampler, 0);
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	// Allow input layout and deny uneccessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// Create an empty root signature.
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	//rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError)))
	{
		__debugbreak();
	}

	if (FAILED(pD3DDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature))))
	{
		__debugbreak();
	}
	if (pSignature)
	{
		pSignature->Release();
		pSignature = nullptr;
	}
	if (pError)
	{
		pError->Release();
		pError = nullptr;
	}
	return TRUE;
}
BOOL CBasicMeshObject::InitPipelineState()
{
	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	CShaderManager* pShaderManager = m_pRenderer->INL_GetShaderManager();

	m_pVS = pShaderManager->CreateShaderDXC(L"shBasicMesh.hlsl", L"VSMain", L"vs_6_0", 0);
	if (!m_pVS)
		__debugbreak();

	m_pVS_Deform = pShaderManager->CreateShaderDXC(L"shBasicMesh.hlsl", L"VSDeform", L"vs_6_0", 0);
	if (!m_pVS_Deform)
		__debugbreak();

	m_pCS_Deform = pShaderManager->CreateShaderDXC(L"shBasicMesh.hlsl", L"CSDeform", L"cs_6_0", 0);
	if (!m_pCS_Deform)
		__debugbreak();

	m_pPS = pShaderManager->CreateShaderDXC(L"shBasicMesh.hlsl", L"PSMain", L"ps_6_0", 0);
	if (!m_pPS)
		__debugbreak();

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	0, 52,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};


	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = m_pRootSignature;
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_pVS->pCodeBuffer, m_pVS->dwCodeSize);
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_pPS->pCodeBuffer, m_pPS->dwCodeSize);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	//psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;	
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	if (FAILED(pD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState))))
	{
		__debugbreak();
	}

	// deform shader
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_pVS_Deform->pCodeBuffer, m_pVS_Deform->dwCodeSize);
	if (FAILED(pD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState_Deform))))
	{
		__debugbreak();
	}


	// for Acceleration Structure
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = m_pRootSignature;
	computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(m_pCS_Deform->pCodeBuffer, m_pCS_Deform->dwCodeSize);
	if (FAILED(pD3DDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_pPipelineState_CS_Deform))))
		__debugbreak();

	return TRUE;
}
BOOL CBasicMeshObject::BeginCreateMesh(const BasicVertex* pVertexList, DWORD dwVertexNum, DWORD dwTriGroupCount)
{
	BOOL bResult = FALSE;
	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	CD3D12ResourceManager*	pResourceManager = m_pRenderer->INL_GetResourceManager();

	if (dwTriGroupCount > MAX_TRI_GROUP_COUNT_PER_OBJ)
		__debugbreak();

	if (FAILED(pResourceManager->CreateVertexBuffer(sizeof(BasicVertex), dwVertexNum, &m_VertexBufferView, &m_pVertexBuffer, (void*)pVertexList)))
	{
		__debugbreak();
		goto lb_return;
	}
	
	m_dwMaxTriGroupCount = dwTriGroupCount;
	m_pTriGroupList = new INDEXED_TRI_GROUP[m_dwMaxTriGroupCount];
	memset(m_pTriGroupList, 0, sizeof(INDEXED_TRI_GROUP) * m_dwMaxTriGroupCount);
	m_dwVertexCount = dwVertexNum;

	bResult = TRUE;

lb_return:
	return bResult;
}
BOOL CBasicMeshObject::InsertIndexedTriList(const DWORD* pIndexList, DWORD dwTriCount, const WCHAR* wchDiffuseTexFileName, const WCHAR* wchNormalTexFileName, MaterialType::Type mtlType, BOOL bUseAlphaTest)
{
	BOOL bResult = FALSE;

	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	UINT srvDescriptorSize = m_pRenderer->INL_GetSrvDescriptorSize();
	CD3D12ResourceManager*	pResourceManager = m_pRenderer->INL_GetResourceManager();
	CSingleDescriptorAllocator* pSingleDescriptorAllocator = m_pRenderer->INL_GetSingleDescriptorAllocator();

	ID3D12Resource* pIndexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};

	if (m_dwTriGroupCount >= m_dwMaxTriGroupCount)
	{
		__debugbreak();
		goto lb_return;
	}
	DWORD dwIndicesSize = dwTriCount * 3 * sizeof(DWORD);
	DWORD dwAlignedIndicesSize = (dwIndicesSize / 16 + ((dwIndicesSize % 16) != 0)) * 16;
	DWORD dwAlignedIndexNum = dwAlignedIndicesSize / sizeof(DWORD);

	if (FAILED(pResourceManager->CreateIndexBuffer(dwAlignedIndexNum, &IndexBufferView, &pIndexBuffer, (void*)pIndexList, sizeof(DWORD) * dwTriCount * 3)))
	{
		__debugbreak();
		goto lb_return;
	}
	INDEXED_TRI_GROUP*	pTriGroup = m_pTriGroupList + m_dwTriGroupCount;
	pTriGroup->pIndexBuffer = pIndexBuffer;
	pTriGroup->IndexBufferView = IndexBufferView;
	pTriGroup->dwTriCount = dwTriCount;
	pTriGroup->dwAlignedIndexCount = dwAlignedIndexNum;
	pTriGroup->pDiffuseTexHandle = (TEXTURE_HANDLE*)m_pRenderer->CreateTextureFromFile(wchDiffuseTexFileName);
	
	pTriGroup->bUseAlphaTest = bUseAlphaTest;

	FillBasicMaterial(&pTriGroup->mtl, mtlType);

	if (wchNormalTexFileName)
	{
		pTriGroup->pNormalTexHandle = (TEXTURE_HANDLE*)m_pRenderer->CreateTextureFromFile(wchNormalTexFileName);
	}
	else
	{
		DWORD dwColor = 0x00ff7f7f;	// (0.0f, 0.0f, 1.0f) -> (0.5f, 0.5f, 1.0) -> (127,128,255)
		pTriGroup->pNormalTexHandle = (TEXTURE_HANDLE*)m_pRenderer->CreateImmutableTexture(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, (const BYTE*)&dwColor);
	}
	
	m_dwTriGroupCount++;
	bResult = TRUE;
lb_return:
	return bResult;
}
void CBasicMeshObject::EndCreateMesh()
{
	// for Acceleration Structure
	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	CSingleDescriptorAllocator* pSingleDescriptorAllocator = m_pRenderer->INL_GetSingleDescriptorAllocator();

	if (!pSingleDescriptorAllocator->AllocDescriptorHandle(&m_srvVertexBuffer))
		__debugbreak();

	UINT VertexSize = sizeof(BasicVertex);

	// Create SRV from VertexBuffer
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = m_dwVertexCount;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Buffer.StructureByteStride = VertexSize;
	srvDesc.Buffer.FirstElement = 0;
	pD3DDevice->CreateShaderResourceView(m_pVertexBuffer, &srvDesc, m_srvVertexBuffer);
}
void* CBasicMeshObject::CreateBLAS(BOOL bAllowUpdate)
{
	BLAS_INSTANCE* pBlasInstance = nullptr;
	CRayTracingManager* pRayTracingManager = m_pRenderer->INL_GetRayTracingManager();


	BLAS_BUILD_TRIGROUP_INFO* pBuildInfoList = new BLAS_BUILD_TRIGROUP_INFO[m_dwTriGroupCount];
	memset(pBuildInfoList, 0, sizeof(BLAS_BUILD_TRIGROUP_INFO) * m_dwTriGroupCount);
	DWORD dwBuildInfoCount = 0;
	for (DWORD i = 0; i < m_dwTriGroupCount; i++)
	{
		pBuildInfoList[dwBuildInfoCount].pIB = m_pTriGroupList[i].pIndexBuffer;
		pBuildInfoList[dwBuildInfoCount].bNotOpaque = FALSE;
		if (m_pTriGroupList[i].bUseAlphaTest)
		{
			pBuildInfoList[dwBuildInfoCount].bNotOpaque = TRUE;
		}
		
		pBuildInfoList[dwBuildInfoCount].dwIndexNum = m_pTriGroupList[i].dwAlignedIndexCount;
		pBuildInfoList[dwBuildInfoCount].pDiffuseTexHandle = m_pTriGroupList[i].pDiffuseTexHandle;
		pBuildInfoList[dwBuildInfoCount].pNormalTexHandle = m_pTriGroupList[i].pNormalTexHandle;
		pBuildInfoList[dwBuildInfoCount].mtl = m_pTriGroupList[i].mtl;
		dwBuildInfoCount++;
	}
	pBlasInstance = pRayTracingManager->AllocBLAS(m_pVertexBuffer, sizeof(BasicVertex), m_dwVertexCount, pBuildInfoList, dwBuildInfoCount, bAllowUpdate);
	delete[] pBuildInfoList;

	return (void*)pBlasInstance;
}
void CBasicMeshObject::DeleteBLAS(void* pBlasHandle)
{
	CRayTracingManager* pRayTracingManager = m_pRenderer->INL_GetRayTracingManager();
	BLAS_INSTANCE* pBlasInstance = (BLAS_INSTANCE*)pBlasHandle;
	pRayTracingManager->FreeBLAS(pBlasInstance);
}
void CBasicMeshObject::Draw(ID3D12GraphicsCommandList6* pCommandList, const XMMATRIX* pMatWorld, BOOL bDeform, float fRadius)
{
	// 각각의 draw()작업의 무결성을 보장하려면 draw() 작업마다 다른 영역의 descriptor table(shader visible)과 다른 영역의 CBV를 사용해야 한다.
	// 따라서 draw()할 때마다 CBV는 ConstantBuffer Pool로부터 할당받고, 렌더리용 descriptor table(shader visible)은 descriptor pool로부터 할당 받는다.

	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	UINT srvDescriptorSize = m_pRenderer->INL_GetSrvDescriptorSize();
	CDescriptorPool* pDescriptorPool = m_pRenderer->INL_GetDescriptorPool();
	ID3D12DescriptorHeap* pDescriptorHeap = pDescriptorPool->INL_GetDescriptorHeap();
	CSimpleConstantBufferPool* pConstantBufferPool = m_pRenderer->GetConstantBufferPool(CONSTANT_BUFFER_TYPE_DEFAULT);
	

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	DWORD dwRequiredDescriptorCount = DESCRIPTOR_COUNT_PER_OBJ + (m_dwTriGroupCount * DESCRIPTOR_COUNT_PER_TRI_GROUP);

	if (!pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, dwRequiredDescriptorCount))
	{
		__debugbreak();
	}

	// 각각의 draw()에 대해 독립적인 constant buffer(내부적으로는 같은 resource의 다른 영역)를 사용한다.
	CB_CONTAINER* pCB = pConstantBufferPool->Alloc();
	if (!pCB)
	{
		__debugbreak();
	}
	CONSTANT_BUFFER_DEFAULT* pConstantBufferDefault = (CONSTANT_BUFFER_DEFAULT*)pCB->pSystemMemAddr;

	DWORD dwFrameCount = m_pRenderer->INL_GetFrameCount();

	// constant buffer의 내용을 설정
	// view/proj matrix
	m_pRenderer->GetViewProjMatrix(&pConstantBufferDefault->matView, &pConstantBufferDefault->matProj);
	
	// world matrix
	pConstantBufferDefault->matWorld = XMMatrixTranspose(*pMatWorld);
	pConstantBufferDefault->fRadis = fRadius;
	pConstantBufferDefault->fInterpolation = sinf((float)dwFrameCount * 0.05f) * 0.5f + 0.5f;

	// Descriptor Table 구성
	// 이번에 사용할 constant buffer의 descriptor를 렌더링용(shader visible) descriptor table에 카피

	// per Obj
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_PER_OBJ_CBV, srvDescriptorSize);
	pD3DDevice->CopyDescriptorsSimple(1, Dest, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);	// cpu측 코드에서는 cpu descriptor handle에만 write가능
	Dest.Offset(1, srvDescriptorSize);

	// per tri-group
	for (DWORD i = 0; i < m_dwTriGroupCount; i++)
	{
		INDEXED_TRI_GROUP* pTriGroup = m_pTriGroupList + i;
		TEXTURE_HANDLE* pTexHandle = pTriGroup->pDiffuseTexHandle;
		if (pTexHandle)
		{
			pD3DDevice->CopyDescriptorsSimple(1, Dest, pTexHandle->srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);	// cpu측 코드에서는 cpu descriptor handle에만 write가능
		}
		else
		{
			__debugbreak();
		}
		Dest.Offset(1, srvDescriptorSize);
	}
	
	// set RootSignature
	pCommandList->SetGraphicsRootSignature(m_pRootSignature);
	pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);

	// ex) when TriGroupCount = 3
	// per OBJ | TriGroup 0 | TriGroup 1 | TriGroup 2 |
	// CBV     |     SRV    |     SRV    |     SRV    | 

	ID3D12PipelineState* pPipelineState = m_pPipelineState;
	if (bDeform)
	{
		pPipelineState = m_pPipelineState_Deform;
	}
	pCommandList->SetPipelineState(pPipelineState);
	
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);

	// set descriptor table for root-param 0
	pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);	// Entry per Obj

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTableForTriGroup(gpuDescriptorTable, DESCRIPTOR_COUNT_PER_OBJ, srvDescriptorSize);
	for (DWORD i = 0; i < m_dwTriGroupCount; i++)
	{
		// set descriptor table for root-param 1
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTableForTriGroup);	// Entry of Tri-Groups
		gpuDescriptorTableForTriGroup.Offset(1, srvDescriptorSize);

		INDEXED_TRI_GROUP* pTriGroup = m_pTriGroupList + i;
		pCommandList->IASetIndexBuffer(&pTriGroup->IndexBufferView);
		pCommandList->DrawIndexedInstanced(pTriGroup->dwTriCount * 3, 1, 0, 0, 0);
	}
}

void CBasicMeshObject::UpdateBLAS(ID3D12GraphicsCommandList6* pCommandList, D3D12_CPU_DESCRIPTOR_HANDLE uavVertexOutput, const XMMATRIX* pMatWorld, float fRadius)
{
	// 각각의 draw()작업의 무결성을 보장하려면 draw() 작업마다 다른 영역의 descriptor table(shader visible)과 다른 영역의 CBV를 사용해야 한다.
	// 따라서 draw()할 때마다 CBV는 ConstantBuffer Pool로부터 할당받고, 렌더리용 descriptor table(shader visible)은 descriptor pool로부터 할당 받는다.

	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	UINT srvDescriptorSize = m_pRenderer->INL_GetSrvDescriptorSize();
	CDescriptorPool* pDescriptorPool = m_pRenderer->INL_GetDescriptorPool();
	ID3D12DescriptorHeap* pDescriptorHeap = pDescriptorPool->INL_GetDescriptorHeap();
	CSimpleConstantBufferPool* pConstantBufferPool = m_pRenderer->GetConstantBufferPool(CONSTANT_BUFFER_TYPE_DEFAULT);
	

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};

	if (!pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, DESCRIPTOR_COUNT_PER_OBJ_UPDATE_VB))
	{
		__debugbreak();
	}

	// 각각의 draw()에 대해 독립적인 constant buffer(내부적으로는 같은 resource의 다른 영역)를 사용한다.
	CB_CONTAINER* pCB = pConstantBufferPool->Alloc();
	if (!pCB)
	{
		__debugbreak();
	}
	CONSTANT_BUFFER_DEFAULT* pConstantBufferDefault = (CONSTANT_BUFFER_DEFAULT*)pCB->pSystemMemAddr;
	DWORD dwFrameCount = m_pRenderer->INL_GetFrameCount();

	// constant buffer의 내용을 설정
	// view/proj matrix
	m_pRenderer->GetViewProjMatrix(&pConstantBufferDefault->matView, &pConstantBufferDefault->matProj);

	// world matrix
	pConstantBufferDefault->matWorld = XMMatrixTranspose(*pMatWorld);
	
	// local vertex tranform
	pConstantBufferDefault->fRadis = fRadius;
	pConstantBufferDefault->fInterpolation = sinf((float)dwFrameCount * 0.05f) * 0.5f + 0.5f;
	pConstantBufferDefault->VertexCount = m_dwVertexCount;
	

	// Descriptor Table 구성
	// 이번에 사용할 constant buffer의 descriptor를 렌더링용(shader visible) descriptor table에 카피

	// per Obj
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dest(cpuDescriptorTable);
	pD3DDevice->CopyDescriptorsSimple(1, Dest, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);	// Constant buffer
	Dest.Offset(1, srvDescriptorSize);
	pD3DDevice->CopyDescriptorsSimple(1, Dest, m_srvVertexBuffer, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);	// Src VertexBuffer(srv)
	Dest.Offset(1, srvDescriptorSize);
	pD3DDevice->CopyDescriptorsSimple(1, Dest, uavVertexOutput, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);	// Dest VertexBuffer(uav)
	Dest.Offset(1, srvDescriptorSize);

	// set RootSignature
	pCommandList->SetComputeRootSignature(m_pRootSignature);
	pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);
	pCommandList->SetPipelineState(m_pPipelineState_CS_Deform);

	// set descriptor table for root-param 0
	pCommandList->SetComputeRootDescriptorTable(0, gpuDescriptorTable);	// Entry per Obj
	DWORD dwGroupNum = (m_dwVertexCount / 1024) + ((m_dwVertexCount % 1024) != 0);
	pCommandList->Dispatch(dwGroupNum, 1, 1);
	int a = 0;
}
void CBasicMeshObject::FillBasicMaterial(BASIC_MATERIAL* pOutMtl, MaterialType::Type mtlType)
{
	pOutMtl->type = mtlType;
	pOutMtl->Ks = XMFLOAT3(0.5f, 0.5f, 0.5f);
	pOutMtl->roughness = 0.01f;
	pOutMtl->Kr = XMFLOAT3(0.5f, 0.5f, 0.5f);
	pOutMtl->Kt = XMFLOAT3(0.0f, 0.0f, 0.0f);
	pOutMtl->type = MaterialType::Default;
	pOutMtl->AmbientIntensity = 0.25f;
	pOutMtl->opacity = XMFLOAT3(1.0f, 1.0f, 1.0f);

	if (MaterialType::Glass == mtlType)
	{
		pOutMtl->Ks = XMFLOAT3(0.1f, 0.1f, 0.1f);
		pOutMtl->Kr = XMFLOAT3(0.05f, 0.05f, 0.05f);
		pOutMtl->Kt = XMFLOAT3(0.95f, 0.95f, 0.95f);
		pOutMtl->opacity = XMFLOAT3(0.5f, 0.5f, 0.5f);
		pOutMtl->AmbientIntensity = 0.01f;
	}

	if (MaterialType::Matte == mtlType)
	{
		pOutMtl->Kr = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
}

void CBasicMeshObject::Cleanup()
{
	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	CSingleDescriptorAllocator* pSingleDescriptorAllocator = m_pRenderer->INL_GetSingleDescriptorAllocator();

	// delete all triangles-group
	
	if (m_pTriGroupList)
	{
		for (DWORD i = 0; i < m_dwTriGroupCount; i++)
		{
			if (m_pTriGroupList[i].pIndexBuffer)
			{
				m_pTriGroupList[i].pIndexBuffer->Release();
				m_pTriGroupList[i].pIndexBuffer = nullptr;
			}
			if (m_pTriGroupList[i].pDiffuseTexHandle)
			{
				m_pRenderer->DeleteTexture(m_pTriGroupList[i].pDiffuseTexHandle);
				m_pTriGroupList[i].pDiffuseTexHandle = nullptr;
			}
			if (m_pTriGroupList[i].pNormalTexHandle)
			{
				m_pRenderer->DeleteTexture(m_pTriGroupList[i].pNormalTexHandle);
				m_pTriGroupList[i].pNormalTexHandle = nullptr;
			}
		}
		delete[] m_pTriGroupList;
		m_pTriGroupList = nullptr;
	}

	if (m_srvVertexBuffer.ptr)
	{
		pSingleDescriptorAllocator->FreeDescriptorHandle(m_srvVertexBuffer);
		m_srvVertexBuffer.ptr = 0;
	}

	if (m_pVertexBuffer)
	{
		m_pVertexBuffer->Release();
		m_pVertexBuffer = nullptr;
	}
	CleanupSharedResources();
}
void CBasicMeshObject::CleanupSharedResources()
{
	CShaderManager* pShaderManager = m_pRenderer->INL_GetShaderManager();

	if (!m_dwInitRefCount)
		return;

	DWORD ref_count = --m_dwInitRefCount;
	if (!ref_count)
	{
		if (m_pRootSignature)
		{
			m_pRootSignature->Release();
			m_pRootSignature = nullptr;
		}
		if (m_pPipelineState)
		{
			m_pPipelineState->Release();
			m_pPipelineState = nullptr;
		}
		if (m_pPipelineState_Deform)
		{
			m_pPipelineState_Deform->Release();
			m_pPipelineState_Deform = nullptr;
		}
		if (m_pPipelineState_CS_Deform)
		{
			m_pPipelineState_CS_Deform->Release();
			m_pPipelineState_CS_Deform = nullptr;
		}
		if (m_pVS)
		{
			pShaderManager->ReleaseShader(m_pVS);
			m_pVS = nullptr;
		}
		if (m_pVS_Deform)
		{
			pShaderManager->ReleaseShader(m_pVS_Deform);
			m_pVS_Deform = nullptr;
		}
		if (m_pCS_Deform)
		{
			pShaderManager->ReleaseShader(m_pCS_Deform);
			m_pCS_Deform = nullptr;
		}
		if (m_pPS)
		{
			pShaderManager->ReleaseShader(m_pPS);
			m_pPS = nullptr;
		}
	}
}
CBasicMeshObject::~CBasicMeshObject()
{
	Cleanup();
}