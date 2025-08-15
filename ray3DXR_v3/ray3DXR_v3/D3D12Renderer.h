#pragma once

#include "Renderer_typedef.h"

struct SHADER_HANDLE;
class CShaderManager;
class CD3D12ResourceManager;
class CDescriptorPool;
class CSimpleConstantBufferPool;
class CSingleDescriptorAllocator;
class CConstantBufferManager;
class CFontManager;
class CTextureManager;
class CRayTracingManager;
class CD3D12Renderer
{
	static const UINT MAX_DRAW_COUNT_PER_FRAME = 1024;
	static const UINT MAX_DESCRIPTOR_COUNT = 4096;

	HWND	m_hWnd = nullptr;
	ID3D12Device5*	m_pD3DDevice = nullptr;
	ID3D12CommandQueue*	m_pCommandQueue = nullptr;
	CD3D12ResourceManager*	m_pResourceManager = nullptr;
	CFontManager*			m_pFontManager = nullptr;
	CSingleDescriptorAllocator* m_pSingleDescriptorAllocator = nullptr;
	
	ID3D12CommandAllocator* m_ppCommandAllocator[MAX_PENDING_FRAME_COUNT] = {};
	ID3D12GraphicsCommandList6* m_ppCommandList[MAX_PENDING_FRAME_COUNT] = {};
	CDescriptorPool*	m_ppDescriptorPool[MAX_PENDING_FRAME_COUNT] = {};
	CConstantBufferManager* m_ppConstBufferManager[MAX_PENDING_FRAME_COUNT] = {};
	CTextureManager*	m_pTextureManager = nullptr;

	UINT64	m_pui64LastFenceValue[MAX_PENDING_FRAME_COUNT] = {};
	UINT64	m_ui64FenceVaule = 0;

	CShaderManager*	m_pShaderManager = nullptr;
	CRayTracingManager* m_pRayTracingManager = nullptr;
	D3D_FEATURE_LEVEL	m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;
	DXGI_ADAPTER_DESC1	m_AdapterDesc = {};
	IDXGISwapChain3*	m_pSwapChain = nullptr;
	D3D12_VIEWPORT	m_Viewport = {};
	D3D12_RECT		m_ScissorRect = {};
	DWORD			m_dwWidth = 0;
	DWORD			m_dwHeight = 0;
	float 			m_fDPI = 96.0f;

	ID3D12Resource*	m_pRenderTargets[SWAP_CHAIN_FRAME_COUNT] = {};
	ID3D12Resource*	m_pDepthStencil = nullptr;

	ID3D12DescriptorHeap*		m_pRTVHeap = nullptr;
	ID3D12DescriptorHeap*		m_pDSVHeap = nullptr;
	ID3D12DescriptorHeap*		m_pSRVHeap = nullptr;	
	UINT	m_rtvDescriptorSize = 0;
	UINT	m_srvDescriptorSize = 0;
	UINT	m_dsvDescriptorSize = 0;
	UINT	m_dwSwapChainFlags = 0;
	UINT	m_uiRenderTargetIndex = 0;
	HANDLE	m_hFenceEvent = nullptr;
	ID3D12Fence* m_pFence = nullptr;
	
	TEXTURE_HANDLE* m_pSkyTex = nullptr;
	DWORD	m_dwCurContextIndex = 0;
	XMMATRIX m_matView = {};
	XMMATRIX m_matViewInv = {};
	XMMATRIX m_matProj = {};

	XMVECTOR m_CamPos = {};
	XMVECTOR m_CamDir = {};
	XMVECTOR m_CamRight = {};
	XMVECTOR m_CamUp = {};
	float m_fCamYaw = 0.0f;
	float m_fCamPitch = 0.0f;
	float m_fCamRoll = 0.0f;
	DWORD m_dwFrameCount = 0;
	
	BOOL	m_bDXREnabled = TRUE;	


	void	InitCamera();

	void	CreateFence();
	void	CleanupFence();
	void	CreateCommandList();
	void	CleanupCommandList();

	BOOL	CreateDescriptorHeapForRTV();
	void	CleanupDescriptorHeapForRTV();
	BOOL	CreateDescriptorHeapForDSV();
	void	CleanupDescriptorHeapForDSV();
	

	BOOL	CreateDepthStencil(UINT Width, UINT Height);
	void	CleanupDepthStencil();

	UINT64	Fence();
	void	WaitForFenceValue(UINT64 ExpectedFenceValue);

	void	Cleanup();
	void	UpdateCamera();
public:
	BOOL	Initialize(HWND hWnd, BOOL bEnableDebugLayer, BOOL bEnableGBV, BOOL bDebugShader, const WCHAR* wchShaderPath, DWORD dwMaxBlasCount);
	void	BeginRender();
	void	EndRender();
	
	void	Present();
	BOOL	UpdateWindowSize(DWORD dwBackBufferWidth, DWORD dwBackBufferHeight);

	void*	CreateBasicMeshObject();
	void	DeleteBasicMeshObject(void* pMeshObjHandle);

	void*	CreateBLAS(void* pMeshObjHandle, BOOL bAllowUpdate);
	void	DeleteBLAS(void* pMeshObjHandle, void* pBlasHandle);
	void	UpdateBLASTransform(void* pBlasHandle, const XMMATRIX* pMatWorld);	// Transform RigidBody(MATRIX in TLAS)
	void	UpdateBLAS(void* pMeshObjHandle, void* pBlasHandle, const XMMATRIX* pMatWorld, float fRadius);	// Transform Vertices

	void*	CreateSpriteObject();
	void*	CreateSpriteObject(const WCHAR* wchTexFileName, int PosX, int PosY, int Width, int Height);
	void	DeleteSpriteObject(void* pSpriteObjHandle);

	void	SetSkyCubeMap(void* pTexHandle);

	BOOL	BeginCreateMesh(void* pMeshObjHandle, const BasicVertex* pVertexList, DWORD dwVertexCount, DWORD dwTriGroupCount);
	BOOL	InsertTriGroup(void* pMeshObjHandle, const DWORD* pIndexList, DWORD dwTriCount, const WCHAR* wchDiffuseTexFileName, const WCHAR* wchNormalTexFileName, MaterialType::Type mtlType, BOOL bUseAlphaTest);
	void	EndCreateMesh(void* pMeshObjHandle);

	void*	CreateTiledTexture(UINT TexWidth, UINT TexHeight, DWORD r, DWORD g, DWORD b);
	void*	CreateDynamicTexture(UINT TexWidth, UINT TexHeight);
	void*	CreateTextureFromFile(const WCHAR* wchFileName);
	void*	CreateImmutableTexture(UINT TexWidth, UINT TexHeight, DXGI_FORMAT format, const BYTE* pInitImage);
	void	DeleteTexture(void* pTexHandle);

	void*	CreateFontObject(const WCHAR* wchFontFamilyName, float fFontSize);
	void	DeleteFontObject(void* pFontHandle);
	BOOL	WriteTextToBitmap(BYTE* pDestImage, UINT DestWidth, UINT DestHeight, UINT DestPitch, int* piOutWidth, int* piOutHeight, void* pFontObjHandle, const WCHAR* wchString, DWORD dwLen);

	void	RenderMeshObject(void* pMeshObjHandle, const XMMATRIX* pMatWorld, BOOL bDeform, float fRadius);
	
	void	RenderSpriteWithTex(void* pSprObjHandle, int iPosX, int iPosY, float fScaleX, float fScaleY, const RECT* pRect, float Z, void* pTexHandle);
	void	RenderSprite(void* pSprObjHandle, int iPosX, int iPosY, float fScaleX, float fScaleY, float Z);
	void	UpdateTextureWithImage(void* pTexHandle, const BYTE* pSrcBits, UINT SrcWidth, UINT SrcHeight);

	void	SetCameraPos(float x, float y, float z);
	void	MoveCamera(float x, float y, float z);
	void	GetCameraPos(float* pfOutX, float* pfOutY, float* pfOutZ);
	

	void	SetCameraRot(float fYaw, float fPitch, float fRoll);
	void	EnableDXR(BOOL bSwitch);
	BOOL	IsEnabledDXR();

	// for internal
	ID3D12Device5* INL_GetD3DDevice() const { return m_pD3DDevice; }
	CD3D12ResourceManager*	INL_GetResourceManager() { return m_pResourceManager; }

	CDescriptorPool*	INL_GetDescriptorPool() { return m_ppDescriptorPool[m_dwCurContextIndex]; }
	CShaderManager* INL_GetShaderManager() { return m_pShaderManager; }
	CRayTracingManager* INL_GetRayTracingManager() { return m_pRayTracingManager; }
	CSimpleConstantBufferPool* GetConstantBufferPool(CONSTANT_BUFFER_TYPE type);

	UINT INL_GetSrvDescriptorSize() { return m_srvDescriptorSize; }
	CSingleDescriptorAllocator* INL_GetSingleDescriptorAllocator() { return m_pSingleDescriptorAllocator; }
	void	FillProjDecompConstant(DECOMP_PROJ* pOutConstBuffer);
	void	FillRayTraceConstant(CONSTANT_BUFFER_RAY_TRACING* pOutBuffer);
	DWORD GetWidth() const { return m_dwWidth; }
	DWORD GetHeight() const { return m_dwHeight; }
	void	GetViewProjMatrix(XMMATRIX* pOutMatView, XMMATRIX* pOutMatProj);
	DWORD	INL_GetScreenWidth() const { return m_dwWidth; }
	DWORD	INL_GetScreenHeigt() const { return m_dwHeight; }
	float	INL_GetDPI() const { return m_fDPI; }
	DWORD	INL_GetFrameCount() const { return m_dwFrameCount; }
	
	CD3D12Renderer();
	~CD3D12Renderer();
};

