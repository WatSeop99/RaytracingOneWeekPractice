#pragma once

struct TextureHandle
{
	ID3D12Resource* pTextureResource;
	ID3D12Resource* pUploadBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle;
	D3D12_GPU_VIRTUAL_ADDRESS GPUHandle;
};

class Application;

class TextureManager
{
public:
	TextureManager() = default;
	~TextureManager() { Cleanup(); }

	bool Initialize(Application* pApp);
	bool Cleanup();

	TextureHandle* CreateNonImageTexture(UINT sizePerElem, UINT numElements);

private:
	HRESULT CreateNonImageBuffer(ID3D12Resource** ppOutResource, UINT sizePerEleme, UINT numElements);

private:
	

	// DO NOT Release directly.
	Application* m_pApp = nullptr;
};
