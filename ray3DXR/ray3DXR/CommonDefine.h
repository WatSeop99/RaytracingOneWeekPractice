#pragma once

#include <d3d12.h>

struct Buffer
{
	ID3D12Resource* pResource;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;
};


