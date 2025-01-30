#include "framework.h"
#include "AccelerationStructure.h"
#include "Application.h"
#include "GPUResource.h"
#include "Material.h"
#include "ResourceManager.h"
#include "Vertex.h"
#include "Object.h"

UINT Quad::ms_QuadCount = 0;
UINT Box::ms_BoxCount = 0;
UINT Sphere::ms_SphereCount = 0;

bool Object::Initialize(const WCHAR* pszNAME, UINT sizePerVertex, UINT numVertex, const void* pVERTICES, UINT sizePerIndex, UINT numIndex, const void* pINDICES, bool bIsLightSource)
{
	_ASSERT(pszNAME);
	_ASSERT(pVERTICES);
	_ASSERT(pINDICES);
	_ASSERT(m_pApp);
	_ASSERT(!m_pGeometryInfo);
	_ASSERT(!m_pConstantBuffer);
	_ASSERT(m_MaterialID != UINT_MAX);

	ID3D12Device5* pDevice = m_pApp->GetDevice();

	m_pGeometryInfo = new BottomLevelAccelerationStructureGeometry;
	if (!m_pGeometryInfo)
	{
		return false;
	}

	// Set name.
	swprintf_s(m_pGeometryInfo->m_szName, MAX_PATH, pszNAME);

	// Set geometry buffer.
	GeometryBuffer geomBuffer;
	{
		ResourceManager* pResourceManager = m_pApp->GetResourceManager();
		Buffer* pVertexBuffer = nullptr;
		Buffer* pIndexBuffer = nullptr;

		pVertexBuffer = pResourceManager->CreateVertexBuffer(sizePerVertex, numVertex, pVERTICES);
		if (!pVertexBuffer)
		{
			return false;
		}

		pIndexBuffer = pResourceManager->CreateIndexBuffer(sizePerIndex, numIndex, pINDICES);
		if (!pIndexBuffer)
		{
			pVertexBuffer->pResource->Release();
			delete pVertexBuffer;
			return false;
		}

		memcpy(&geomBuffer.VertexBuffer, pVertexBuffer, sizeof(Buffer));
		memcpy(&geomBuffer.IndexBuffer, pIndexBuffer, sizeof(Buffer));
		m_pGeometryInfo->m_Geometries.push_back(geomBuffer);
		m_pGeometryInfo->m_VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		m_pGeometryInfo->m_IndexFormat = DXGI_FORMAT_R32_UINT;

		delete pVertexBuffer;
		delete pIndexBuffer;
	}

	// Set default geometry instance.
	{
		GeometryInstance geomInstance;
		geomInstance.VB.StartIndex = 0;
		geomInstance.VB.Count = numVertex;
		geomInstance.VB.GPUDescriptorHandle = geomBuffer.VertexBuffer.GPUHandle;
		geomInstance.VB.VertexBuffer.StartAddress = geomBuffer.VertexBuffer.pResource->GetGPUVirtualAddress();
		geomInstance.VB.VertexBuffer.StrideInBytes = sizePerVertex;
		geomInstance.IB.StartIndex = 0;
		geomInstance.IB.Count = numIndex;
		geomInstance.IB.GPUDescriptorHandle = geomBuffer.IndexBuffer.GPUHandle;
		geomInstance.IB.IndexBuffer = geomBuffer.IndexBuffer.pResource->GetGPUVirtualAddress();
		geomInstance.Transform = 0;
		geomInstance.GeometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

		m_pGeometryInfo->m_GeometryInstances.push_back(geomInstance);
	}

	// Add bottom-level AS.
	AccelerationStructureManager* pASManager = nullptr;
	if (bIsLightSource)
	{
		pASManager = m_pApp->GetLightASManager();
	}
	else
	{
		pASManager = m_pApp->GetASManager();
	}
	if (!pASManager->AddBottomLevelAS(pDevice, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE, m_pGeometryInfo, false, true))
	{
		return false;
	}

	// Create constant buffer.
	m_pConstantBuffer = new ConstantBuffer;
	if (!m_pConstantBuffer)
	{
		return false;
	}

	ObjectCommonCB initData = { m_MaterialID, FALSE, };
	return m_pConstantBuffer->Initialize(m_pApp, sizeof(ObjectCommonCB), &initData);
}

bool Object::Cleanup()
{
	if (m_pGeometryInfo)
	{
		delete m_pGeometryInfo;
		m_pGeometryInfo = nullptr;
	}
	if (m_pConstantBuffer)
	{
		delete m_pConstantBuffer;
		m_pConstantBuffer = nullptr;
	}
	m_pApp = nullptr;

	return true;
}

bool Quad::Initialize(Application* pApp, float width, float height, UINT materialID, const DirectX::XMFLOAT4X4 TRANSFORM, bool bIsLightSource)
{
	_ASSERT(pApp);
	_ASSERT(width > 0.0f);
	_ASSERT(height > 0.0f);
	_ASSERT(materialID != UINT_MAX);

	m_pApp = pApp;
	m_MaterialID = materialID;
	
	if (ms_QuadCount == 0)
	{
		DirectX::XMFLOAT3 normal = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);

		float halfWidth = width * 0.5f;
		float halfHeight = height * 0.5f;
		Vertex vertices[4] =
		{
			{ DirectX::XMFLOAT3(-halfWidth, -halfHeight, 0.0f), normal, DirectX::XMFLOAT2(0.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, -halfHeight, 0.0f), normal, DirectX::XMFLOAT2(0.0f, 1.0f) },
			{ DirectX::XMFLOAT3(-halfWidth, halfHeight, 0.0f), normal, DirectX::XMFLOAT2(1.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, halfHeight, 0.0f), normal, DirectX::XMFLOAT2(1.0f, 1.0f) },
		};
		UINT indices[6] = { 0, 1, 2, 1, 3, 2 };

		if (!Object::Initialize(L"Quad", sizeof(Vertex), _countof(vertices), vertices, sizeof(UINT), _countof(indices), indices, bIsLightSource))
		{
			return false;
		}
	}
	++ms_QuadCount;

	// Set bottom-level instance.
	AccelerationStructureManager* pASManager = (bIsLightSource ? m_pApp->GetLightASManager() : m_pApp->GetASManager());
	return (pASManager->AddBottomLevelASInstance(L"Quad", UINT_MAX, DirectX::XMLoadFloat4x4(&TRANSFORM)) != -1);
}

bool Box::Initialize(Application* pApp, float width, float height, float depth, UINT materialID, const DirectX::XMFLOAT4X4 TRANSFORM, bool bIsLightSource)
{
	_ASSERT(pApp);
	_ASSERT(width > 0.0f);
	_ASSERT(height > 0.0f);
	_ASSERT(depth > 0.0f);
	_ASSERT(materialID != UINT_MAX);

	m_pApp = pApp;
	m_MaterialID = materialID;

	if (ms_BoxCount == 0)
	{
		float halfWidth = width * 0.5f;
		float halfHeight = height * 0.5f;
		float halfDepth = depth * 0.5f;
		Vertex vertices[24] =
		{
			// front 0~3
			{ DirectX::XMFLOAT3(-halfWidth, -halfHeight, -halfDepth), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, -halfHeight, -halfDepth), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
			{ DirectX::XMFLOAT3(-halfWidth, halfHeight, -halfDepth), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, halfHeight, -halfDepth), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },

			// back 4~7
			{ DirectX::XMFLOAT3(halfWidth, halfHeight, halfDepth), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
			{ DirectX::XMFLOAT3(-halfWidth, halfHeight, halfDepth), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
			{ DirectX::XMFLOAT3(halfWidth, -halfHeight, halfDepth), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
			{ DirectX::XMFLOAT3(-halfWidth, -halfHeight, halfDepth), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },
			 
			// up 8~11
			{ DirectX::XMFLOAT3(-halfWidth, halfHeight, halfDepth), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, halfHeight, halfDepth), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
			{ DirectX::XMFLOAT3(-halfWidth, halfHeight, -halfDepth), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, halfHeight, -halfDepth), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },

			// down 12~15
			{ DirectX::XMFLOAT3(-halfWidth, -halfHeight, -halfDepth), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, -halfHeight, -halfDepth), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
			{ DirectX::XMFLOAT3(-halfWidth, -halfHeight, halfDepth), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, -halfHeight, halfDepth), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },

			// left 16~19
			{ DirectX::XMFLOAT3(-halfWidth, halfHeight, halfDepth), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
			{ DirectX::XMFLOAT3(-halfWidth, halfHeight, -halfDepth), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
			{ DirectX::XMFLOAT3(-halfWidth, -halfHeight, halfDepth), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
			{ DirectX::XMFLOAT3(-halfWidth, -halfHeight, -halfDepth), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },

			// right 20~23
			{ DirectX::XMFLOAT3(halfWidth, halfHeight, -halfDepth), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, halfHeight, halfDepth), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
			{ DirectX::XMFLOAT3(halfWidth, -halfHeight, -halfDepth), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
			{ DirectX::XMFLOAT3(halfWidth, -halfHeight, halfDepth), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },
		};
		UINT indices[36] =
		{
			0, 1, 2, 1, 3, 2,
			4, 5, 6, 5, 7, 6,
			8, 9, 10, 9, 11, 10,
			12, 13, 14, 13, 15, 14,
			16, 17, 18, 17, 19, 18,
			20, 21, 22, 21, 23, 22
		};

		if (!Object::Initialize(L"Box", sizeof(Vertex), _countof(vertices), vertices, sizeof(UINT), _countof(indices), indices, bIsLightSource))
		{
			return false;
		}
	}
	++ms_BoxCount;

	// Set bottom-level instance.
	AccelerationStructureManager* pASManager = nullptr;
	if (bIsLightSource)
	{
		pASManager = m_pApp->GetLightASManager();
	}
	else
	{
		pASManager = m_pApp->GetASManager();
	}
	return pASManager->AddBottomLevelASInstance(L"Box", UINT_MAX, DirectX::XMLoadFloat4x4(&TRANSFORM));
}

bool Sphere::Initialize(Application* pApp, float radius, UINT materialID, const DirectX::XMFLOAT4X4 TRANSFORM, bool bIsLightSource)
{
	_ASSERT(pApp);
	_ASSERT(radius > 0.0f);
	_ASSERT(materialID != UINT_MAX);

	m_pApp = pApp;
	m_MaterialID = materialID;
	m_Radius = radius;


	if (ms_SphereCount == 0)
	{
		const int NUM_SLICES = 30;
		const int NUM_STACKS = 30;

		const float D_THETA = -DirectX::XM_2PI / (float)NUM_SLICES;
		const float D_PHI = -DirectX::XM_PI / (float)NUM_STACKS;

		Vertex vertices[(NUM_STACKS + 1) * (NUM_SLICES + 1)];
		UINT indices[NUM_SLICES * NUM_STACKS * 6];

		for (int j = 0; j <= NUM_STACKS; ++j)
		{
			// 스택에 쌓일 수록 시작점을 x-y 평면에서 회전 시켜서 위로 올리는 구조
			DirectX::XMFLOAT3 start(0.0f, -m_Radius, 0.0f);
			DirectX::XMVECTOR stackStartPoint = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&start), DirectX::XMMatrixRotationZ(D_PHI * j));
			
			for (int i = 0; i <= NUM_SLICES; ++i)
			{
				Vertex& v = vertices[j * (NUM_SLICES + 1) + i];

				DirectX::XMVECTOR temp;
				DirectX::XMFLOAT4X4 tempMat;

				// 시작점을 x-z 평면에서 회전시키면서 원을 만드는 구조.
				temp = DirectX::XMVector3Transform(stackStartPoint, DirectX::XMMatrixRotationY(D_THETA * (float)i));
				DirectX::XMStoreFloat3(&v.Position, temp);

				v.Normal = v.Position; // 원점이 구의 중심.
				temp = DirectX::XMLoadFloat3(&v.Normal);
				temp = DirectX::XMVector3Normalize(temp);
				DirectX::XMStoreFloat3(&v.Normal, temp);

				v.TexCoord = DirectX::XMFLOAT2((float)i / NUM_SLICES, 1.0f - (float)j / NUM_STACKS);
				/*temp = DirectX::XMLoadFloat2(&v.TexCoord);
				temp = DirectX::XMVectorMultiply(temp, DirectX::XMVectorReplicate(1.0f));
				DirectX::XMStoreFloat2(&v.TexCoord, temp);*/
			}
		}

		int pushIndex = 0;
		for (int j = 0; j < NUM_STACKS; ++j)
		{
			const int OFFSET = (NUM_SLICES + 1) * j;

			for (int i = 0; i < NUM_SLICES; ++i)
			{
				indices[pushIndex++] = OFFSET + i;
				indices[pushIndex++] = OFFSET + i + NUM_SLICES + 1;
				indices[pushIndex++] = OFFSET + i + 1 + NUM_SLICES + 1;

				indices[pushIndex++] = OFFSET + i;
				indices[pushIndex++] = OFFSET + i + 1 + NUM_SLICES + 1;
				indices[pushIndex++] = OFFSET + i + 1;
			}
		}
		_ASSERT(pushIndex == NUM_SLICES * NUM_STACKS * 6);

		if (!Object::Initialize(L"Sphere", sizeof(Vertex), _countof(vertices), vertices, sizeof(UINT), _countof(indices), indices, bIsLightSource))
		{
			return false;
		}
	}
	++ms_SphereCount;

	// Set bottom-level instance.
	AccelerationStructureManager* pASManager = nullptr;
	if (bIsLightSource)
	{
		pASManager = m_pApp->GetLightASManager();
	}
	else
	{
		pASManager = m_pApp->GetASManager();
	}
	return pASManager->AddBottomLevelASInstance(L"Sphere", UINT_MAX, DirectX::XMLoadFloat4x4(&TRANSFORM));
}
