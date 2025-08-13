#pragma once

struct D3DRESOURCE_LINK
{
	SORT_LINK*	pHead;
	SORT_LINK*	pTail;
	DWORD	dwSize;
	DWORD	dwResourceCount;
};


struct D3DRESOURCE_ALLOC_DESC
{
	ID3D12Resource*	pResource;
	ULONGLONG	RegisteredTick;
	SORT_LINK	Link;
	DWORD	dwSize;
	int	iFrameLifeCount;
};


class CD3DResourceRecycleBin
{
	ID3D12Device5* m_pD3DDevice = nullptr;
	D3DRESOURCE_LINK m_pResourceLink[16] = {};

	SORT_LINK*	m_pPedingResourceLinkHead = nullptr;
	SORT_LINK*	m_pPedingResourceLinkTail = nullptr;

	D3D12_HEAP_TYPE m_HeapType = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_FLAGS m_ResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES m_InitialResouceState = D3D12_RESOURCE_STATE_GENERIC_READ;

	ULONGLONG	m_PrvFreeedTick = 0;
	WCHAR	m_wchResourceName[64] = {};

	D3DRESOURCE_LINK* FindLink(UINT Size);
	DWORD UpdatePendingResource(ULONGLONG CurTick);
	void FreeAllExpiredD3DResource(ULONGLONG CurTick);
	void Cleanup();
public:
	void Initialize(ID3D12Device5* pD3DDevice, D3D12_HEAP_TYPE HeapType, D3D12_RESOURCE_FLAGS ResourceFlags, D3D12_RESOURCE_STATES InitialResourceState, const WCHAR* wchResourceName);
	ID3D12Resource* Alloc(UINT Size);
	void Free(ID3D12Resource* pResource, int iFrameLifeCount);
	void Update(ULONGLONG CurTick);

	CD3DResourceRecycleBin();
	~CD3DResourceRecycleBin();
};

