#pragma once

class CIndexCreator;
class CShaderTable;
class CD3D12Renderer;
class CSingleDescriptorAllocator;
class CD3DResourceRecycleBin;
class CRayTracingManager
{
	enum COMMON_DESCRIPTOR_INDEX
	{
		COMMON_DESCRIPTOR_INDEX_OUTPUT_DIFFUSE_UAV,	// UAV - Output - Diffuse
		COMMON_DESCRIPTOR_INDEX_OUTPUT_DEPTH_UAV,	// UAV - Output - Depth
		COMMON_DESCRIPTOR_COUNT,
	};
	enum DISPATCH_DESCRIPTOR_INDEX
	{
		DISPATCH_DESCRIPTOR_INDEX_RAYTRACING_CBV,
		DISPATCH_DESCRIPTOR_INDEX_OUTPUT_DIFFUSE,
		DISPATCH_DESCRIPTOR_INDEX_OUTPUT_DEPTH,
		DISPATCH_DESCRIPTOR_INDEX_SRV_SKY_CUBEMAP,
		DISPATCH_DESCRIPTOR_INDEX_COUNT,
	};	
	enum LOCAL_ROOT_PARAM_DESCRIPTOR_INDEX
	{
		LOCAL_ROOT_PARAM_DESCRIPTOR_INDEX_VB,	// VB
		LOCAL_ROOT_PARAM_DESCRIPTOR_INDEX_IB,	// IB
		LOCAL_ROOT_PARAM_DESCRIPTOR_INDEX_TEX_DIFFUSE,	// SRV - Texture - Diffuse
		LOCAL_ROOT_PARAM_DESCRIPTOR_INDEX_TEX_NORMAL,	// SRV - Texture - Normal
		LOCAL_ROOT_PARAM_DESCRIPTOR_COUNT
	};

	enum UPDATE_ACCELERATION_STRCTURE_TYPE
	{
		UPDATE_ACCELERATION_STRCTURE_TYPE_HIT_GROUP_SHADER_TABLE = 0b0001,
		UPDATE_ACCELERATION_STRCTURE_TYPE_TLAS = 0b0010,
		UPDATE_ACCELERATION_STRCTURE_TYPE_BLAS = 0b0100
	};
	static const DWORD MAX_RECURSION_DEPTH = 3;
	static const DWORD MAX_RADIANCE_RECURSION_DEPTH = min(MAX_RECURSION_DEPTH, 3);
	static const DWORD MAX_SHADOW_RECURSION_DEPTH = min(MAX_RECURSION_DEPTH, 2);


	CD3D12Renderer* m_pRenderer = nullptr;
	ID3D12Device5*	m_pD3DDevice = nullptr;
	CD3DResourceRecycleBin* m_pResourceBinTLAS = nullptr;
	CD3DResourceRecycleBin* m_pResourceBinBLAS = nullptr;
	CD3DResourceRecycleBin* m_pResourceBinScratchResource = nullptr;
	CD3DResourceRecycleBin* m_pResourceBinTLASInstanceDescList = nullptr;
	CD3DResourceRecycleBin* m_pResourceBinVBResource = nullptr;

	CIndexCreator*	m_pIndexCreator = nullptr;
	ID3D12Resource*	m_pOutputDiffuse = nullptr;	// raytracing output - diffuse
	ID3D12Resource*	m_pOutputDepth = nullptr;	// raytracing output - depth	
	DWORD m_dwWidth = 0;
	DWORD m_dwHeight = 0;
	
	DWORD	m_dwMaxShaderVisibleDescritporCount = 0;
	DWORD	m_dwMaxBlasCount = 0;
	BLAS_INSTANCE** m_ppCollectedBlasInstanceList = nullptr;

	SHADER_HANDLE*	m_pRayShader = nullptr;
	ID3D12RootSignature*	m_pRaytracingGlobalRootSignature = nullptr;
	ID3D12RootSignature*	m_pRaytracingLocalRootSignature = nullptr;
	ID3D12StateObject*		m_pDXRStateObject = nullptr;
	
	ID3D12DescriptorHeap*	m_pCommonDescriptorHeap = nullptr;
	ID3D12DescriptorHeap*	m_pShaderVisibleDescriptorHeap = nullptr;	// ID에 따라 srv,uav 위치 고정.
	UINT	m_DescriptorSize = 0;
	CShaderTable* m_pRayGenShaderTable = nullptr;
	CShaderTable* m_pMissShaderTable = nullptr;
	CShaderTable* m_pHitGroupShaderTable = nullptr;
	UINT	m_MissShaderTableStrideInBytes = 0;
	UINT	m_HitGroupShaderTableStrideInBytes = 0;
	DWORD	m_dwHitGroupShaderRecordNum = 0;
	UINT m_ShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	UINT m_HitGroupShaderRecordSize = 0;

	
	SORT_LINK*	m_pBlasInstanceLinkHead = nullptr;
	SORT_LINK*	m_pBlasInstanceLinkTail = nullptr;
	DWORD		m_dwBlasCount = 0;
	ID3D12Resource* m_pBLASInstanceDescResouce = nullptr;
	ID3D12Resource* m_pTLAS = nullptr;

	DWORD	m_dwUpdateAccelerationStructureFlags = 0;
	//BOOL m_bMustUpdateAccelerationStructure = FALSE;


	void	BuildShaderTables();
	void	CleanupShaderTables();

	void	CreateRootSignatures();
	void	CreateRaytracingPipelineStateObject();
	
	void	CreateDescriptorHeapCBV_SRV_UAV();
	void	CleanupDescriptorHeapForCBV_SRV_UAV();

	void	CreateShaderVisibleHeap(DWORD dwMaxDescriptorCount);
	void	CleanupDispatchHeap();

	BOOL	CreateOutputDiffuseBuffer(UINT Width, UINT Height);
	void	CleanupOutputDiffuseBuffer();
	BOOL	CreateOutputDepthBuffer(UINT Width, UINT Height);
	void	CleanupOutputDepthBuffer();
	void	Cleanup();

	BOOL	BuildBLAS(ID3D12GraphicsCommandList6* pCommandList, BLAS_INSTANCE* pBlasInstance);
	BOOL	UpdateBLAS(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pBLAS, const D3D12_RAYTRACING_GEOMETRY_DESC* pGeomDescList, DWORD dwGeomCount);
	ID3D12Resource*	BuildTLAS(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pInstanceDescResource, BLAS_INSTANCE** ppInstanceList, DWORD dwBlasInstanceNum, BOOL bAllowUpdate, UINT CurContextIndex);

	void	UpdateHitGroupShaderTable(DWORD dwShaderRecordCount);
	void	FreeAllBLAS();
public:
	BOOL	Initialize(CD3D12Renderer* pRenderer, DWORD dwWidth, DWORD dwHeight, DWORD dwMaxBlasCount);
	void	DoRaytracing(ID3D12GraphicsCommandList6* pCommandList, D3D12_CPU_DESCRIPTOR_HANDLE srvSkyCubeMap);
	BOOL	IsUpdatedAccelerationStructure() const { return (BOOL)(m_dwUpdateAccelerationStructureFlags != 0); };
	BOOL	UpdateAccelerationStructure(ID3D12GraphicsCommandList6* pCommandList);
	void	UpdateWindowSize(DWORD dwWidth, DWORD dwHeight);
	void	UpdateManagedResource();
	BLAS_INSTANCE*	AllocBLAS(ID3D12Resource* pVertexBuffer, UINT VertexSize, DWORD dwVertexCount, const BLAS_BUILD_TRIGROUP_INFO* pTriGroupInfoList, DWORD dwTriGroupInfoCount, BOOL bAllowUpdate);
	void	FreeBLAS(BLAS_INSTANCE* pBlasInstance);
	void	UpdateBLASTransform(BLAS_INSTANCE* pBlasInstance, const XMMATRIX* pMatWorld);
	void	SetUpdateBLAS(BLAS_INSTANCE* pBlasInstance);
	
	ID3D12Resource*	INL_GetOutputResource() { return m_pOutputDiffuse; }
	ID3D12Resource* INL_GetDepthResource() { return m_pOutputDepth; }
	

	CRayTracingManager();
	~CRayTracingManager();
};