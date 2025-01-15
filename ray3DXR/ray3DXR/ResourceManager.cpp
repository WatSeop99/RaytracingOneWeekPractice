#include "framework.h"
#include "Application.h"
#include "CommonDefine.h"
#include "ResourceManager.h"

bool ResourceManager::Initialize(Application* pApp)
{
	bool bRet = false;

	if (!pApp)
	{
		goto LB_RET;
	}

	m_pDevice = pApp->GetDevice();
	m_pDevice->AddRef();

	m_pCommandQueue = pApp->GetCommandQueue();
	m_pCommandQueue->AddRef();

	HRESULT hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator));
	if (FAILED(hr))
	{
		goto LB_RET;
	}

	hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList));
	if (FAILED(hr))
	{
		goto LB_RET;
	}

	bRet = true;

LB_RET:
	return bRet;
}

bool ResourceManager::Cleanup()
{
	bool bRet = false;

	if (m_pCommandList)
	{
		m_pCommandList->Release();
		m_pCommandList = nullptr;
	}
	if (m_pCommandAllocator)
	{
		m_pCommandAllocator->Release();
		m_pCommandAllocator = nullptr;
	}
	if (m_pCommandQueue)
	{
		m_pCommandQueue->Release();
		m_pCommandQueue = nullptr;
	}
	if (m_pDevice)
	{
		m_pDevice->Release();
		m_pDevice = nullptr;
	}

	m_pApp = nullptr;

	bRet = true;

LB_RET:
	return bRet;
}

Buffer* ResourceManager::CreateVertexBuffer(UINT sizePerVertex, UINT numVertex, void* pInitData)
{
	Buffer* pRetBuffer = new Buffer;
	if (!pRetBuffer)
	{
		goto LB_RET;
	}
	ZeroMemory(pRetBuffer, sizeof(Buffer));

	ID3D12Resource* pResource = nullptr;
	ID3D12Resource* pUploadResource = nullptr;

	UINT bufferSize = sizePerVertex * numVertex;
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
	HRESULT hr = m_pDevice->CreateCommittedResource(&heapProperties,
													D3D12_HEAP_FLAG_NONE,
													&resourceDesc,
													D3D12_RESOURCE_STATE_COMMON,
													nullptr,
													IID_PPV_ARGS(&pResource));
	if (FAILED(hr))
	{
		delete pRetBuffer;
		pRetBuffer = nullptr;
		goto LB_RET;
	}


	if (pInitData)
	{
		m_pCommandAllocator->Reset();
		m_pCommandList->Reset(m_pCommandAllocator, nullptr);

		heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		hr = m_pDevice->CreateCommittedResource(&heapProperties,
												D3D12_HEAP_FLAG_NONE,
												&resourceDesc,
												D3D12_RESOURCE_STATE_COMMON,
												nullptr,
												IID_PPV_ARGS(&pUploadResource));
		if (FAILED(hr))
		{
			delete pRetBuffer;
			pRetBuffer = nullptr;
			goto LB_RET;
		}


		BYTE* pVertexDataBegin = nullptr;
		CD3DX12_RANGE readRange(0, 0);

		const CD3DX12_RESOURCE_BARRIER BEFORE_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(pResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		const CD3DX12_RESOURCE_BARRIER AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(pResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		m_pCommandList->ResourceBarrier(1, &BEFORE_BARRIER);
		m_pCommandList->CopyBufferRegion(pResource, 0, pUploadResource, 0, bufferSize);
		m_pCommandList->ResourceBarrier(1, &AFTER_BARRIER);

		m_pCommandList->Close();

		ID3D12CommandList* ppCommandLists[1] = { m_pCommandList };
		m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);

		UINT64 lastFenceValue = m_pApp->Fence();
		m_pApp->WaitForGPU(lastFenceValue);

		pUploadResource->Release();
		pUploadResource = nullptr;
	}

	pRetBuffer->pResource = pResource;

LB_RET:
	return pRetBuffer;
}

Buffer* ResourceManager::CreateIndexBuffer()
{
	return nullptr;
}
