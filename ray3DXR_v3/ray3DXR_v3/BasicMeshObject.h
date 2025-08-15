#pragma once

struct TEXTURE_HANDLE;
struct SHADER_HANDLE;
class CD3D12Renderer;

enum BASIC_MESH_DESCRIPTOR_INDEX_PER_OBJ
{
	BASIC_MESH_DESCRIPTOR_INDEX_PER_OBJ_CBV = 0
};
enum BASIC_MESH_DESCRIPTOR_INDEX_PER_TRI_GROUP
{
	BASIC_MESH_DESCRIPTOR_INDEX_PER_TRI_GROUP_TEX = 0
};

struct INDEXED_TRI_GROUP
{
	ID3D12Resource* pIndexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};
	DWORD	dwTriCount;
	DWORD	dwAlignedIndexCount;
	TEXTURE_HANDLE* pDiffuseTexHandle;
	TEXTURE_HANDLE* pNormalTexHandle;
	BASIC_MATERIAL	mtl;
	BOOL	bUseAlphaTest;
};

class CBasicMeshObject
{
public:
	static const UINT DESCRIPTOR_COUNT_PER_OBJ = 1;			// | Constant Buffer
	static const UINT DESCRIPTOR_COUNT_PER_TRI_GROUP = 1;	// | SRV(tex)
	static const UINT MAX_TRI_GROUP_COUNT_PER_OBJ = 8;
	static const UINT MAX_DESCRIPTOR_COUNT_FOR_DRAW = DESCRIPTOR_COUNT_PER_OBJ + (MAX_TRI_GROUP_COUNT_PER_OBJ * DESCRIPTOR_COUNT_PER_TRI_GROUP);

	static const UINT DESCRIPTOR_COUNT_PER_OBJ_UPDATE_VB = 3;	// | Constant Buffer | Src VertexBuffer(SRV) | Dest VertexBuffer(UAV)
private:
	// shared by all CBasicMeshObject instances.
	static SHADER_HANDLE* m_pVS;
	static SHADER_HANDLE* m_pVS_Deform;
	static SHADER_HANDLE* m_pCS_Deform;
	static SHADER_HANDLE* m_pPS;
	static ID3D12RootSignature* m_pRootSignature;
	static ID3D12PipelineState* m_pPipelineState;
	static ID3D12PipelineState* m_pPipelineState_Deform;
	static ID3D12PipelineState* m_pPipelineState_CS_Deform;
	static DWORD	m_dwInitRefCount;

	CD3D12Renderer* m_pRenderer = nullptr;
	
	// vertex data
	ID3D12Resource* m_pVertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};

	INDEXED_TRI_GROUP*	m_pTriGroupList = nullptr;
	DWORD	m_dwTriGroupCount = 0;
	DWORD	m_dwMaxTriGroupCount = 0;
	DWORD	m_dwVertexCount = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE	m_srvVertexBuffer = {};

	BOOL	InitCommonResources();
	void	CleanupSharedResources();

	BOOL	InitRootSinagture();
	BOOL	InitPipelineState();

	void	Cleanup();
	void	FillBasicMaterial(BASIC_MATERIAL* pOutMtl, MaterialType::Type mtlType);

public:
	BOOL	Initialize(CD3D12Renderer* pRenderer);
	void	Draw(ID3D12GraphicsCommandList6* pCommandList, const XMMATRIX* pMatWorld, BOOL bDeform, float fRadius);
	void	UpdateBLAS(ID3D12GraphicsCommandList6* pCommandList, D3D12_CPU_DESCRIPTOR_HANDLE uavVertexOutput, const XMMATRIX* pMatWorld, float fRadius);

	BOOL	BeginCreateMesh(const BasicVertex* pVertexList, DWORD dwVertexNum, DWORD dwTriGroupCount);
	BOOL	InsertIndexedTriList(const DWORD* pIndexList, DWORD dwTriCount, const WCHAR* wchDiffuseTexFileName, const WCHAR* wchNormalTexFileName, MaterialType::Type mtlType, BOOL bUseAlphaTest);
	void	EndCreateMesh();

	void*	CreateBLAS(BOOL bAllowUpdate);
	void	DeleteBLAS(void* pBlasHandle);
	CBasicMeshObject();
	~CBasicMeshObject();
};

