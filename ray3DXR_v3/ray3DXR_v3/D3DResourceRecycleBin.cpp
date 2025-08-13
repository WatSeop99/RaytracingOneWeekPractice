#include "pch.h"
#include <d3d12.h>
#include <d3dx12.h>
#include "typedef.h"
#include "D3DUtil.h"
#include "IndexCreator.h"
#include "D3D12ResourceManager.h"
#include "D3D12Renderer.h"
#include "D3DResourceRecycleBin.h"

CD3DResourceRecycleBin::CD3DResourceRecycleBin()
{
}

void CD3DResourceRecycleBin::Initialize(ID3D12Device5* pD3DDevice, D3D12_HEAP_TYPE HeapType, D3D12_RESOURCE_FLAGS ResourceFlags, D3D12_RESOURCE_STATES InitialResourceState, const WCHAR* wchResourceName)
{
	m_pD3DDevice = pD3DDevice;
	
	DWORD	dwMaxBufferCount = (DWORD)_countof(m_pResourceLink);
	DWORD	dwBufferSize = 65536;
	for (DWORD i = 0; i < dwMaxBufferCount; i++)
	{
		m_pResourceLink[i].dwSize = dwBufferSize;
		dwBufferSize *= 2;
	}
	m_HeapType = HeapType;
	m_ResourceFlags = ResourceFlags;
	m_InitialResouceState = InitialResourceState;
	wcscpy_s(m_wchResourceName, wchResourceName);
}

D3DRESOURCE_LINK* CD3DResourceRecycleBin::FindLink(UINT Size)
{
	DWORD	dwMaxBufferCount = (DWORD)_countof(m_pResourceLink);
	D3DRESOURCE_LINK*	pSelectedLink = nullptr;

	for (DWORD i = 0; i < dwMaxBufferCount; i++)
	{
		D3DRESOURCE_LINK*	pLink = m_pResourceLink + i;
		if (Size <= pLink->dwSize)
		{
			pSelectedLink = pLink;
			break;
		}
	}
	return pSelectedLink;
}

ID3D12Resource* CD3DResourceRecycleBin::Alloc(UINT Size)
{
	ID3D12Resource*	pResource = nullptr;
	D3DRESOURCE_LINK*	pLink = FindLink(Size);
	if (!pLink)
		__debugbreak();

	if (pLink->pHead)
	{
		D3DRESOURCE_ALLOC_DESC*	pDesc = (D3DRESOURCE_ALLOC_DESC*)pLink->pHead->pItem;
		UnLinkFromLinkedList(&pLink->pHead, &pLink->pTail, &pDesc->Link);
		pResource = pDesc->pResource;

		D3D12_RESOURCE_DESC desc = pResource->GetDesc();
		if (desc.Width < Size)
			__debugbreak();

		delete pDesc;
		pLink->dwResourceCount--;
		goto lb_return;
	}
	if (pLink->dwSize < Size)
		__debugbreak();
	
	CD3DX12_HEAP_PROPERTIES HeapProp = CD3DX12_HEAP_PROPERTIES(m_HeapType);
	CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(pLink->dwSize, m_ResourceFlags);

	if (FAILED(m_pD3DDevice->CreateCommittedResource(
		&HeapProp,
		D3D12_HEAP_FLAG_NONE,
		&BufferDesc,
		m_InitialResouceState,
		nullptr,
		IID_PPV_ARGS(&pResource))))
	{
		__debugbreak();
	}
	pResource->SetName(m_wchResourceName);
lb_return:
	return pResource;
}
void CD3DResourceRecycleBin::Free(ID3D12Resource* pResource, int iFrameLifeCount)
{
	D3D12_RESOURCE_DESC desc = pResource->GetDesc();

	D3DRESOURCE_ALLOC_DESC*	pDesc = new D3DRESOURCE_ALLOC_DESC;
	pDesc->dwSize = (DWORD)desc.Width;
	pDesc->Link.pItem = pDesc;
	pDesc->Link.pNext = nullptr;
	pDesc->Link.pPrv = nullptr;
	pDesc->pResource = pResource;
	pDesc->iFrameLifeCount = iFrameLifeCount;

	LinkToLinkedListFIFO(&m_pPedingResourceLinkHead, &m_pPedingResourceLinkTail, &pDesc->Link);
}

void CD3DResourceRecycleBin::Update(ULONGLONG CurTick)
{
	// Free()로 대기 링크에 들어간 리소스들은 대기 프레임 수 만큼 경과하면 사이즈별 할당 링크로 이동
	UpdatePendingResource(CurTick);

	// 할당 링크에 존재하는 리소스들도 등록된지 일정 시간이 경과하면 제거한다.
	ULONGLONG ElapsedTick = CurTick - m_PrvFreeedTick;
	if (ElapsedTick > 1000)
	{
		// 1초마다 시간이 경과된 리소스들을 실제로 release
		FreeAllExpiredD3DResource(CurTick);
		m_PrvFreeedTick = CurTick;
	}
}
DWORD CD3DResourceRecycleBin::UpdatePendingResource(ULONGLONG CurTick)
{
	//
	// 대기 프레임 수 만큼 경과한 리소스들을 대기 링크에서 빼내서  사이즈별 할당 링크로 이동
	//
	DWORD dwResourceCount = 0;	// 대기링크에 남아있는 리소스 개수

	SORT_LINK* pCur = m_pPedingResourceLinkHead;
	SORT_LINK* pNext = nullptr;
	while (pCur)
	{
		pNext = pCur->pNext;
		dwResourceCount++;

		D3DRESOURCE_ALLOC_DESC* pDesc = (D3DRESOURCE_ALLOC_DESC*)pCur->pItem;

		if (pDesc->iFrameLifeCount <= 0)
			__debugbreak();

		int iLifeCount = --pDesc->iFrameLifeCount;
		if (0 == iLifeCount)
		{
			// 대기 링크에서 제거
			UnLinkFromLinkedList(&m_pPedingResourceLinkHead, &m_pPedingResourceLinkTail, &pDesc->Link);

			// 해당 리소스의 사이즈가 모여있는 링크에 추가
			D3DRESOURCE_LINK* pLink = FindLink((UINT)pDesc->dwSize);
			if (!pLink)
				__debugbreak();

			pDesc->RegisteredTick = CurTick;	// 휴지통에 넣은 시간 기록
			pLink->dwResourceCount++;
			LinkToLinkedListFIFO(&pLink->pHead, &pLink->pTail, &pDesc->Link);
			dwResourceCount--;
		}
		pCur = pNext;
	}
	return dwResourceCount;
}
void CD3DResourceRecycleBin::FreeAllExpiredD3DResource(ULONGLONG CurTick)
{
	const ULONGLONG FREE_EXPIRED_D3DRESOURCE_TICK = 1000 * 60;	// 일단 등록후 1분 경과하면 제거

	DWORD	dwMaxBufferCount = (DWORD)_countof(m_pResourceLink);
	for (DWORD i = 0; i < dwMaxBufferCount; i++)
	{
		D3DRESOURCE_LINK*	pLink = m_pResourceLink + i;
		SORT_LINK*	pCur = pLink->pHead;
		SORT_LINK*	pNext = nullptr;
		while (pCur)
		{
			pNext = pCur->pNext;

			D3DRESOURCE_ALLOC_DESC*	pDesc = (D3DRESOURCE_ALLOC_DESC*)pCur->pItem;
			if (CurTick - pDesc->RegisteredTick > FREE_EXPIRED_D3DRESOURCE_TICK)
			{
				UnLinkFromLinkedList(&pLink->pHead, &pLink->pTail, &pDesc->Link);
				pDesc->pResource->Release();
				pDesc->pResource = nullptr;
			#ifdef _DEBUG
				WriteDebugStringW(DEBUG_OUTPUT_TYPE_DEBUG_CONSOLE, L"D3DResource(%u Bytes) released\n", pDesc->dwSize);
			#endif
				delete pDesc;
				pLink->dwResourceCount--;

			}
			pCur = pNext;
		}
	}
}
void CD3DResourceRecycleBin::Cleanup()
{
	ULONGLONG CurTick = GetTickCount64();
	
	while (1)
	{
		// 대기 링크에 존재하는 리소스들을 모두 사이즈별 링크로 이동
		DWORD dwPendingResourceCount = UpdatePendingResource(CurTick);
		if (!dwPendingResourceCount)
			break;
	}

	// 사이즈별 링크에 들어있는 리소스들을 모두 해제
	DWORD	dwMaxBufferCount = (DWORD)_countof(m_pResourceLink);
	for (DWORD i = 0; i < dwMaxBufferCount; i++)
	{
		D3DRESOURCE_LINK*	pLink = m_pResourceLink + i;
		while (pLink->pHead)
		{
			D3DRESOURCE_ALLOC_DESC*	pDesc = (D3DRESOURCE_ALLOC_DESC*)pLink->pHead->pItem;
			UnLinkFromLinkedList(&pLink->pHead, &pLink->pTail, &pDesc->Link);
			pDesc->pResource->Release();
			pDesc->pResource = nullptr;
			delete pDesc;
			pLink->dwResourceCount--;
		}
	}
}
CD3DResourceRecycleBin::~CD3DResourceRecycleBin()
{
	Cleanup();
}