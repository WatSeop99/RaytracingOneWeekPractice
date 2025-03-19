#pragma once

class GPUUploadBuffer
{
public:
    inline ID3D12Resource* GetResource() { return m_pResource; }

protected:
    GPUUploadBuffer() = default;
    virtual ~GPUUploadBuffer()
    {
        if (m_pResource)
        {
            m_pResource->Unmap(0, nullptr);
            m_pResource->Release();
            m_pResource = nullptr;
        }
    }

    void Allocate(ID3D12Device* pDevice, UINT bufferSize, LPCWSTR pszRresourceName = nullptr);

    BYTE* MapCPUWriteOnly();

protected:
    ID3D12Resource* m_pResource = nullptr;
};

