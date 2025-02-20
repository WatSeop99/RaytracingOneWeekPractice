//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
using Microsoft::WRL::ComPtr;

class HrException : public std::runtime_error
{
    inline std::string HrToString(HRESULT hr)
    {
        char s_str[64] = {};
        sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
        return std::string(s_str);
    }
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

inline void ThrowIfFailed(HRESULT hr, const wchar_t* msg)
{
    if (FAILED(hr))
    {
        OutputDebugString(msg);
        throw HrException(hr);
    }
}

inline void ThrowIfFalse(bool value)
{
    ThrowIfFailed(value ? S_OK : E_FAIL);
}

inline void ThrowIfFalse(bool value, const wchar_t* msg)
{
    ThrowIfFailed(value ? S_OK : E_FAIL, msg);
}


inline void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, UINT pathSize)
{
    if (path == nullptr)
    {
        throw std::exception();
    }

    DWORD size = GetModuleFileName(nullptr, path, pathSize);
    if (size == 0 || size == pathSize)
    {
        // Method failed or path was truncated.
        throw std::exception();
    }

    WCHAR* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash)
    {
        *(lastSlash + 1) = L'\0';
    }
}

inline HRESULT ReadDataFromFile(LPCWSTR filename, byte** data, UINT* size)
{
    using namespace Microsoft::WRL;

    CREATEFILE2_EXTENDED_PARAMETERS extendedParams = {};
    extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
    extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
    extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
    extendedParams.lpSecurityAttributes = nullptr;
    extendedParams.hTemplateFile = nullptr;

    Wrappers::FileHandle file(CreateFile2(filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, &extendedParams));
    if (file.Get() == INVALID_HANDLE_VALUE)
    {
        throw std::exception();
    }

    FILE_STANDARD_INFO fileInfo = {};
    if (!GetFileInformationByHandleEx(file.Get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        throw std::exception();
    }

    if (fileInfo.EndOfFile.HighPart != 0)
    {
        throw std::exception();
    }

    *data = reinterpret_cast<byte*>(malloc(fileInfo.EndOfFile.LowPart));
    *size = fileInfo.EndOfFile.LowPart;

    if (!ReadFile(file.Get(), *data, fileInfo.EndOfFile.LowPart, nullptr, nullptr))
    {
        throw std::exception();
    }

    return S_OK;
}

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{}
#endif

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

inline UINT Align(UINT size, UINT alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

inline UINT CalculateConstantBufferByteSize(UINT byteSize)
{
    // Constant buffer size is required to be aligned.
    return Align(byteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
}

#ifdef D3D_COMPILE_STANDARD_FILE_INCLUDE
inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    UINT compileFlags = 0;
#if defined(_DEBUG) || defined(DBG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr;

    Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                            entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
    {
        OutputDebugStringA((char*)errors->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    return byteCode;
}
#endif

// Resets all elements in a ComPtr array.
template<class T>
void ResetComPtrArray(T* comPtrArray)
{
    for (auto& i : *comPtrArray)
    {
        i.Reset();
    }
}


// Resets all elements in a unique_ptr array.
template<class T>
void ResetUniquePtrArray(T* uniquePtrArray)
{
    for (auto& i : *uniquePtrArray)
    {
        i.reset();
    }
}

class GPUUploadBuffer
{
public:
    ID3D12Resource* GetResource() { return m_pResource; }

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

    void Allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr)
    {
        CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);

        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_pResource)));
        m_pResource->SetName(resourceName);
    }

    UINT8* MapCpuWriteOnly()
    {
        UINT8* pMappedData = nullptr;
        // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_pResource->Map(0, &readRange, reinterpret_cast<void**>(&pMappedData)));
        return pMappedData;
    }

protected:
    ID3D12Resource* m_pResource = nullptr;
};

struct D3DBuffer
{
    ID3D12Resource* pResource;
    D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHandle;
};

// Helper class to create and update a constant buffer with proper constant buffer alignments.
// Usage: ToDo
//    ConstantBuffer<...> cb;
//    cb.Create(...);
//    cb.staging.var = ...; | cb->var = ... ; 
//    cb.CopyStagingToGPU(...);
template <class T>
class ConstantBuffer : public GPUUploadBuffer
{
public:
    ConstantBuffer() = default;

    void Create(ID3D12Device* pDevice, UINT numInstances = 1, LPCWSTR resourceName = nullptr)
    {
        m_NumInstances = numInstances;
        UINT alignedSize = Align(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        UINT bufferSize = numInstances * alignedSize;
        Allocate(pDevice, bufferSize, resourceName);
        m_pMappedConstantData = MapCpuWriteOnly();
    }

    inline void CopyStagingToGpu(UINT instanceIndex = 0)
    {
        memcpy(m_pMappedConstantData + instanceIndex * m_AlignedInstanceSize, &staging, sizeof(T));
    }

    // Accessors
    
    inline T* operator->() { return &staging; }
    inline UINT NumInstances() { return m_NumInstances; }
    inline D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress(UINT instanceIndex = 0)
    {
        return m_pResource->GetGPUVirtualAddress() + instanceIndex * m_AlignedInstanceSize;
    }

public:
    T staging;

private:
    UINT8* m_pMappedConstantData = nullptr;
    UINT m_AlignedInstanceSize = 0;
    UINT m_NumInstances = 0;
};


// Helper class to create and update a structured buffer.
// Usage: ToDo
//    ConstantBuffer<...> cb;
//    cb.Create(...);
//    cb.staging.var = ...; | cb->var = ... ; 
//    cb.CopyStagingToGPU(...);
template <class T>
class StructuredBuffer : public GPUUploadBuffer
{
public:
    // Performance tip: Align structures on sizeof(float4) boundary.
    // Ref: https://developer.nvidia.com/content/understanding-structured-buffer-performance
    static_assert(sizeof(T) % 16 == 0, L"Align structure buffers on 16 byte boundary for performance reasons.");

    StructuredBuffer() = default;

    void Create(ID3D12Device* pDevice, UINT numElements, UINT numInstances = 1, LPCWSTR resourceName = nullptr)
    {
        m_Staging.resize(numElements);
        UINT bufferSize = numInstances * numElements * sizeof(T);
        Allocate(pDevice, bufferSize, resourceName);
        m_pMappedBuffers = reinterpret_cast<T*>(MapCpuWriteOnly());
    }

    inline void CopyStagingToGpu(UINT instanceIndex = 0)
    {
        memcpy(m_pMappedBuffers + instanceIndex, &m_Staging[0], InstanceSize());
    }

    // Accessors
    inline T& operator[](UINT elementIndex) { return m_Staging[elementIndex]; }
    inline SIZE_T NumElementsPerInstance() { return m_Staging.size(); }
    inline UINT NumInstances() { return m_Staging.size(); }
    inline SIZE_T InstanceSize() { return NumElementsPerInstance() * sizeof(T); }
    inline D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(UINT instanceIndex = 0)
    {
        return m_pResource->GetGPUVirtualAddress() + instanceIndex * InstanceSize();
    }

private:
    T* m_pMappedBuffers = nullptr;
    std::vector<T> m_Staging;
    UINT m_NumInstances = 0;
};