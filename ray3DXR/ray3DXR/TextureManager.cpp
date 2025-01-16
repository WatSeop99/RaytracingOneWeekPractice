#include "framework.h"
#include "Application.h"
#include "DescriptorAllocator.h"
#include "ResourceManager.h"
#include "TextureManager.h"

bool TextureManager::Initialize(Application* pApp)
{
	if (!pApp)
	{
		return false;
	}

	m_pApp = pApp;
	return true;
}

bool TextureManager::Cleanup()
{
	m_pApp = nullptr;
	return true;
}

TextureHandle* TextureManager::CreateNonImageTexture(UINT sizePerElem, UINT numElements)
{
	_ASSERT(sizePerElem > 0);
	_ASSERT(numElements > 0);

	ID3D12Device5* pDevice = m_pApp->GetDevice();
	DescriptorAllocator* pSRVAllocator = m_pApp->GetCBVSRVUAVAllocator();
	TextureHandle* pRetHandle = nullptr;

	ID3D12Resource* pResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = {};

	HRESULT hr = CreateNonImageBuffer(&pResource, sizePerElem, numElements);
	if (SUCCEEDED(hr))
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = numElements;
		srvDesc.Buffer.StructureByteStride = sizePerElem;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		if (pSRVAllocator->AllocDescriptorHandle(&srvHandle))
		{
			pDevice->CreateShaderResourceView(pResource, &srvDesc, srvHandle);

			pRetHandle = new TextureHandle;
			ZeroMemory(pRetHandle, sizeof(TextureHandle));
			pRetHandle->pTextureResource = pResource;
			pRetHandle->SRVHandle = srvHandle;
			pRetHandle->GPUHandle = pResource->GetGPUVirtualAddress();
		}
		else
		{
			pResource->Release();
			pResource = nullptr;
		}
	}

	return pRetHandle;
}

HRESULT TextureManager::CreateNonImageBuffer(ID3D12Resource** ppOutResource, UINT sizePerElem, UINT numElements)
{
	_ASSERT(ppOutResource);
	_ASSERT(sizePerElem > 0);
	_ASSERT(numElements > 0);

	ID3D12Device5* pDevice = m_pApp->GetDevice();
	ID3D12Resource* pResource = nullptr;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = sizePerElem * numElements;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
	HRESULT hr = pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pResource));
	if (FAILED(hr))
	{
		goto LB_RET;
	}

	*ppOutResource = pResource;

LB_RET:
	return hr;
}
