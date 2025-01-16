#pragma once

#include "BaseForm.h"

struct FrameObject
{
	ID3D12CommandAllocator* pCommandAllocator;
	ID3D12Resource* pSwapChainBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle;
};
struct HeapData
{
	ID3D12DescriptorHeap* pHeap;
	UINT UsedEntries;
};
struct AccelerationStructureBuffers
{
	ID3D12Resource* pScratch;
	ID3D12Resource* pResult;
	ID3D12Resource* pInstanceDesc;    // Used only for top-level AS
};
struct RootSignatureDesc
{
	D3D12_ROOT_SIGNATURE_DESC Desc;
	std::vector<D3D12_DESCRIPTOR_RANGE> Range;
	std::vector<D3D12_ROOT_PARAMETER> RootParams;
};
struct DXILLibrary
{
	DXILLibrary(ID3DBlob* pBlob, const WCHAR* pszENTRY_POINT[], UINT entryPointCount) : pShaderBlob(pBlob)
	{
		_ASSERT(pszENTRY_POINT);

		StateSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		StateSubObject.pDesc = &DXILLibDesc;

		DXILLibDesc = {};
		ExportDesc.resize(entryPointCount);
		ExportName.resize(entryPointCount);
		if (pBlob)
		{
			DXILLibDesc.DXILLibrary.pShaderBytecode = pBlob->GetBufferPointer();
			DXILLibDesc.DXILLibrary.BytecodeLength = pBlob->GetBufferSize();
			DXILLibDesc.NumExports = entryPointCount;
			DXILLibDesc.pExports = ExportDesc.data();

			for (UINT i = 0; i < entryPointCount; ++i)
			{
				ExportName[i] = pszENTRY_POINT[i];
				ExportDesc[i].Name = ExportName[i].c_str();
				ExportDesc[i].Flags = D3D12_EXPORT_FLAG_NONE;
				ExportDesc[i].ExportToRename = nullptr;
			}
		}
	}
	DXILLibrary() : DXILLibrary(nullptr, nullptr, 0) {}
	~DXILLibrary()
	{
		if (pShaderBlob)
		{
			pShaderBlob->Release();
			pShaderBlob = nullptr;
		}
	}

	D3D12_DXIL_LIBRARY_DESC DXILLibDesc;
	D3D12_STATE_SUBOBJECT StateSubObject;
	ID3DBlob* pShaderBlob;
	std::vector<D3D12_EXPORT_DESC> ExportDesc;
	std::vector<std::wstring> ExportName;
};
struct HitProgram
{
	HitProgram(const WCHAR* pszASH_EXPORT, const WCHAR* pszCHS_EXPORT, const std::wstring& NAME) : ExportName(NAME)
	{
		Desc = {};
		Desc.AnyHitShaderImport = pszASH_EXPORT;
		Desc.ClosestHitShaderImport = pszCHS_EXPORT;
		Desc.HitGroupExport = ExportName.c_str();

		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		SubObject.pDesc = &Desc;
	}

	std::wstring ExportName;
	D3D12_HIT_GROUP_DESC Desc;
	D3D12_STATE_SUBOBJECT SubObject;
};
struct ExportAssociation
{
	ExportAssociation(const WCHAR* pszEXPORT_NAMES[], UINT exportCount, const D3D12_STATE_SUBOBJECT* pSUB_OBJECT_TO_ASSOCIATE)
	{
		Association.NumExports = exportCount;
		Association.pExports = pszEXPORT_NAMES;
		Association.pSubobjectToAssociate = pSUB_OBJECT_TO_ASSOCIATE;

		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		SubObject.pDesc = &Association;
	}

	D3D12_STATE_SUBOBJECT SubObject;
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION Association;
};
struct ShaderConfig
{
	ShaderConfig(UINT maxAttributeSizeInBytes, UINT maxPayloadSizeInBytes)
	{
		ShaderConfigure.MaxAttributeSizeInBytes = maxAttributeSizeInBytes;
		ShaderConfigure.MaxPayloadSizeInBytes = maxPayloadSizeInBytes;

		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		SubObject.pDesc = &ShaderConfigure;
	}

	D3D12_RAYTRACING_SHADER_CONFIG ShaderConfigure;
	D3D12_STATE_SUBOBJECT SubObject;
};
struct PipelineConfig
{
	PipelineConfig(UINT maxTraceRecursionDepth)
	{
		Config.MaxTraceRecursionDepth = maxTraceRecursionDepth;

		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		SubObject.pDesc = &Config;
	}

	D3D12_RAYTRACING_PIPELINE_CONFIG Config;
	D3D12_STATE_SUBOBJECT SubObject;
};

static const WCHAR* pszRAY_GEN_SHADER = L"RayGen";
static const WCHAR* pszMISS_SHADER = L"Miss";
static const WCHAR* pszCLOSEST_HIT_SHADER = L"ClosestHit";
static const WCHAR* pszHIT_GROUP = L"HitGroup";

class DescriptorAllocator;
class ResourceManager;
class TextureManager;

class Application final : public BaseForm
{
public:
	static const UINT SWAP_CHAIN_BUFFER = 3;
	static const UINT RTV_HEAP_SIZE = 3;
	static const UINT CBV_SRV_UAV_HEAP_SIZE = 2;

public:
	Application(HINSTANCE hInstance) : BaseForm(hInstance) {}
	~Application() = default;

	void OnLoad() override;
	void OnFrameRender() override;
	void OnShutdown() override;

	UINT64 Fence();
	void WaitForGPU(UINT64 expectedValue);

	inline ID3D12Device5* GetDevice() { return m_pDevice; }
	inline ID3D12CommandQueue* GetCommandQueue() { return m_pCommandQueue; }
	inline ResourceManager* GetResourceManager() { return m_pResourceManager; }
	inline TextureManager* GetTextureManager() { return m_pTextureManager; }
	inline DescriptorAllocator* GetRTVAllocator() { return m_pRTVAllocator; }
	inline DescriptorAllocator* GetCBVSRVUAVAllocator() { return m_pCBVSRVUAVAllocator; }

private:
	void InitDXR();
	
	UINT BeginFrame();
	void EndFrame();

	void CreateAccelerationStructures();
	void CreateRTPipelineState();
	void CreateShaderTable();
	void CreateShaderResources();

	ID3D12Device5* CreateDevice(IDXGIFactory4* pDXGIFactory);
	ID3D12CommandQueue* CreateCommandQueue(ID3D12Device5* pDevice);
	IDXGISwapChain3* CreateSwapChain(IDXGIFactory4* pFactory, HWND hwnd, UINT width, UINT height, DXGI_FORMAT format, ID3D12CommandQueue* pCommandQueue);
	ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device5* pDevice, UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool bShaderVisible);
	D3D12_CPU_DESCRIPTOR_HANDLE CreateRTV(ID3D12Device5* pDevice, ID3D12Resource* pResource, ID3D12DescriptorHeap* pHeap, UINT* outUsedHeapEntries, DXGI_FORMAT format);

	ID3D12Resource* CreateTriangleVB(ID3D12Device5* pDevice);
	AccelerationStructureBuffers CreateBottomLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCommandList, ID3D12Resource* pVertexBuffer);
	AccelerationStructureBuffers CreateTopLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCommandList, ID3D12Resource* pBottomLevelAS, UINT64* pTlasSize);
	ID3D12Resource* CreateBuffer(ID3D12Device5* pDevice, UINT64 size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps);
	ID3D12RootSignature* CreateRootSignature(ID3D12Device5* pDevice, const D3D12_ROOT_SIGNATURE_DESC& DESC);

	DXILLibrary CreateDXILLibrary();
	RootSignatureDesc CreateRayGenRootDesc();
	ID3DBlob* CompileLibrary(const WCHAR* pszFILE_NAME, const WCHAR* pszTARGET_STRING);

	UINT64 SubmitCommandList();
	void ResourceBarrier(ID3D12GraphicsCommandList4* pCommandList, ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

private:
	ID3D12Device5* m_pDevice = nullptr;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	IDXGISwapChain3* m_pSwapChain = nullptr;
	ID3D12GraphicsCommandList4* m_pCommandList = nullptr;
	ID3D12Fence* m_pFence = nullptr;
	HANDLE m_hFenceEvent = nullptr;
	UINT64 m_FenceValue = 0;

	FrameObject m_FrameObjects[SWAP_CHAIN_BUFFER] = { 0, };
	UINT m_FrameIndex = 0;

	HeapData m_RTVHeap = { 0, };

	ID3D12Resource* m_pVertexBuffer = nullptr;
	ID3D12Resource* m_pTopLevelAS = nullptr;
	ID3D12Resource* m_pBottomLevelAS = nullptr;
	UINT64 m_TlasSize = 0;

	ID3D12StateObject* m_pPipelineState = nullptr;
	ID3D12RootSignature* m_pEmptyRootSignature = nullptr;

	ID3D12Resource* m_pShaderTable = nullptr;
	UINT m_ShaderTableEntrySize = 0;

	ID3D12Resource* m_pOutputResource = nullptr;
	ID3D12DescriptorHeap* m_pCBVSRVUAVHeap = nullptr;

	ResourceManager* m_pResourceManager = nullptr;
	TextureManager* m_pTextureManager = nullptr;

	DescriptorAllocator* m_pRTVAllocator = nullptr;
	DescriptorAllocator* m_pCBVSRVUAVAllocator = nullptr;
};
