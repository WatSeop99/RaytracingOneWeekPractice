#include "pch.h"
#include <Windows.h>
#include <DirectXMath.h>
#include "VertexUtil.h"
#include "D3D12Renderer.h"
#include "Game.h"
#include "GameObject.h"

CGameObject::CGameObject()
{
	m_LinkInGame.pItem = this;
	m_LinkInGame.pNext = nullptr;
	m_LinkInGame.pPrv = nullptr;

	m_matScale = XMMatrixIdentity();
	m_matRot = XMMatrixIdentity();
	m_matTrans = XMMatrixIdentity();
	m_matWorld = XMMatrixIdentity();
}
BOOL CGameObject::Initialize(CGame* pGame, GAME_OBJECT_ACTION_TYPE actionType)
{
	BOOL bResult = FALSE;
	CGame* m_pGame = pGame;
	m_pRenderer = pGame->INL_GetRenderer();
	m_ActionType = actionType;

	return bResult;
}
void CGameObject::UpdateTransform()
{
	// world matrix = scale x rotation x trasnlation
	m_matWorld = XMMatrixMultiply(m_matScale, m_matRot);
	m_matWorld = XMMatrixMultiply(m_matWorld, m_matTrans);
	
	if (m_pBlasHandle)
	{	
		m_pRenderer->UpdateBLASTransform(m_pBlasHandle, &m_matWorld);
	}
	m_bUpdateTransform = FALSE;
}
void CGameObject::GetPosition(float* pfOutX, float* pfOutY, float* pfOutZ)
{
	*pfOutX = m_Pos.m128_f32[0];
	*pfOutY = m_Pos.m128_f32[1];
	*pfOutZ = m_Pos.m128_f32[2];
}
void CGameObject::GetScale(float* pfOutX, float* pfOutY, float* pfOutZ)
{
	*pfOutX = m_Scale.m128_f32[0];
	*pfOutY = m_Scale.m128_f32[1];
	*pfOutZ = m_Scale.m128_f32[2];
}
float CGameObject::GetRotationY()
{
	return m_fRotY;
}
void CGameObject::SetPosition(float x, float y, float z)
{
	m_Pos.m128_f32[0] = x;
	m_Pos.m128_f32[1] = y;
	m_Pos.m128_f32[2] = z;

	m_matTrans = XMMatrixTranslation(x, y, z);
	
	m_bUpdateTransform = TRUE;
}
void CGameObject::SetScale(float x, float y, float z)
{
	m_Scale.m128_f32[0] = x;
	m_Scale.m128_f32[1] = y;
	m_Scale.m128_f32[2] = z;

	m_matScale = XMMatrixScaling(x, y, z);
	
	m_bUpdateTransform = TRUE;
}
void CGameObject::SetRotationY(float fRotY)
{
	m_fRotY = fRotY;
	m_matRot = XMMatrixRotationY(fRotY);

	m_bUpdateTransform = TRUE;
}

void CGameObject::Run(DWORD dwFrameCount)
{
	const float MOVE_OFFSET = 0.05f;
	const float SCALE_OFFSET = 0.025f;

	switch (m_ActionType)
	{
		case GAME_OBJECT_ACTION_TYPE_MOVE_X:
			{
				if (m_MovedOffset.x > 1.0f)
				{
					m_fMoveSign = -1.0f;
				}
				else if (m_MovedOffset.x < -1.0f)
				{
					m_fMoveSign = 1.0f;
				}
				float move_x = MOVE_OFFSET * m_fMoveSign;
				
				XMFLOAT3 Pos;
				GetPosition(&Pos.x, &Pos.y, &Pos.z);
				
				Pos.x += move_x;
				m_MovedOffset.x += move_x;
				
				SetPosition(Pos.x, Pos.y, Pos.z);
			}
			break;
		case GAME_OBJECT_ACTION_TYPE_MOVE_Y:
			{
				if (m_MovedOffset.y > 1.0f)
				{
					m_fMoveSign = -1.0f;
				}
				else if (m_MovedOffset.y < -1.0f)
				{
					m_fMoveSign = 1.0f;
				}
				float move_y = MOVE_OFFSET * m_fMoveSign;
				
				XMFLOAT3 Pos;
				GetPosition(&Pos.x, &Pos.y, &Pos.z);
				
				Pos.y += move_y;
				m_MovedOffset.y += move_y;
				
				SetPosition(Pos.x, Pos.y, Pos.z);
			}
			break;
		case GAME_OBJECT_ACTION_TYPE_MOVE_Z:
			{
				if (m_MovedOffset.z > 1.0f)
				{
					m_fMoveSign = -1.0f;
				}
				else if (m_MovedOffset.z < -1.0f)
				{
					m_fMoveSign = 1.0f;
				}
				float move_z = MOVE_OFFSET * m_fMoveSign;
				
				XMFLOAT3 Pos;
				GetPosition(&Pos.x, &Pos.y, &Pos.z);
				
				Pos.z += move_z;
				m_MovedOffset.z += move_z;
				
				SetPosition(Pos.x, Pos.y, Pos.z);
			}
			break;
		case GAME_OBJECT_ACTION_TYPE_ROT_Y:
			{
				float rad = GetRotationY();
				rad += 0.01f;
				if (rad >= 2.0f * 3.1415f)
				{
					rad = 0.0f;
				}
				SetRotationY(rad);
			}
			break;
		case GAME_OBJECT_ACTION_TYPE_SCALE_X:
			{
				XMFLOAT3 Scale;
				GetScale(&Scale.x, &Scale.y, &Scale.z);

				if (Scale.x > 1.5f)
				{
					m_fScaleSign = -1.0f;
				}
				else if (Scale.x < 0.5f)
				{
					m_fScaleSign = 1.0f;
				}
				Scale.x += (SCALE_OFFSET * m_fScaleSign);				
				
				SetScale(Scale.x, Scale.y, Scale.z);
			}
			break;
		case GAME_OBJECT_ACTION_TYPE_SCALE_Y:
			{
				XMFLOAT3 Scale;
				GetScale(&Scale.x, &Scale.y, &Scale.z);

				if (Scale.y > 1.5f)
				{
					m_fScaleSign = -1.0f;
				}
				else if (Scale.y < 0.5f)
				{
					m_fScaleSign = 1.0f;
				}
				Scale.y += (SCALE_OFFSET * m_fScaleSign);				
				
				SetScale(Scale.x, Scale.y, Scale.z);
			}
			break;
		case GAME_OBJECT_ACTION_TYPE_SCALE_Z:
			{
				XMFLOAT3 Scale;
				GetScale(&Scale.x, &Scale.y, &Scale.z);

				if (Scale.z > 1.5f)
				{
					m_fScaleSign = -1.0f;
				}
				else if (Scale.z < 0.5f)
				{
					m_fScaleSign = 1.0f;
				}
				Scale.z += (SCALE_OFFSET * m_fScaleSign);				
				
				SetScale(Scale.x, Scale.y, Scale.z);
			}
			break;
	}
	// per 30FPS or 60 FPS
	if (m_bUpdateTransform)
	{
		UpdateTransform();
	}
	else
	{
		int a = 0;
	}
}
void CGameObject::Render()
{
	if (m_pMeshObj)
	{
		if (m_pRenderer->IsEnabledDXR())
		{
			// raytracing mode
			if (m_bDeformable)
			{
				m_pRenderer->UpdateBLAS(m_pMeshObj, m_pBlasHandle, &m_matWorld, m_fHalfLen);
			}
		}
		else
		{
			// raster mode
			m_pRenderer->RenderMeshObject(m_pMeshObj, &m_matWorld, m_bDeformable, m_fHalfLen);
		}
	}
}

void* CGameObject::CreateBoxMeshObject()
{
	// create box mesh
	// create vertices and indices
	DWORD	pIndexList[36] = {};
	BasicVertex* pVertexList = nullptr;

	float fScale = (float)((rand() % 4) + 1);
	DWORD dwVertexCount = CreateBoxMesh(&pVertexList, pIndexList, (DWORD)_countof(pIndexList), 0.25f * fScale);

	// create BasicMeshObject from Renderer
	m_pMeshObj = m_pRenderer->CreateBasicMeshObject();

	const WCHAR* wchDiffuseTexFileNameList[6] =
	{
		L"tex_00.dds",
		L"tex_01.dds",
		L"tex_02.dds",
		L"tex_03.dds",
		L"tex_04.dds",
		L"tex_05.dds"
	};
	const WCHAR* wchNormalTexFileNameList[6] =
	{
		L"tex_00_N.dds",
		L"tex_01_N.dds",
		L"tex_02_N.dds",
		L"tex_03_N.dds",
		L"tex_04_N.dds",
		L"tex_05_N.dds"
	};


	// Set meshes to the BasicMeshObject
	m_pRenderer->BeginCreateMesh(m_pMeshObj, pVertexList, dwVertexCount, 6);	// 박스의 6면-1면당 삼각형 2개-인덱스 6개
	for (DWORD i = 0; i < 6; i++)
	{
		DWORD dwTexIndex = rand() % 6;
		m_pRenderer->InsertTriGroup(m_pMeshObj, pIndexList + i * 6, 2, wchDiffuseTexFileNameList[dwTexIndex], wchNormalTexFileNameList[dwTexIndex], MaterialType::Default, FALSE);
	}
	m_pRenderer->EndCreateMesh(m_pMeshObj);

	// delete vertices and indices
	if (pVertexList)
	{
		DeleteBoxMesh(pVertexList);
		pVertexList = nullptr;
	}
	if (m_pMeshObj)
	{
		m_pBlasHandle = m_pRenderer->CreateBLAS(m_pMeshObj, FALSE);
	}
	m_ObjectType = GAME_OBJECT_TYPE_BOX;
	return m_pMeshObj;
}

void* CGameObject::CreateGridBoxMeshObject()
{
	// create box mesh
	// create vertices and indices
	DWORD* pIndexList = nullptr;
	DWORD dwIndexCount = 0;
	BasicVertex* pVertexList = nullptr;

	const DWORD PLANE_COUNT = 6;

	float fScale = (float)((rand() % 4) + 1);
	m_fHalfLen = 0.25f * fScale;
	DWORD dwVertexCount = CreateGridBox(&pVertexList, &pIndexList, &dwIndexCount, 8, 8, m_fHalfLen);

	// create BasicMeshObject from Renderer
	m_pMeshObj = m_pRenderer->CreateBasicMeshObject();

	const WCHAR* wchDiffuseTexFileNameList[6] =
	{
		L"tex_00.dds",
		L"tex_01.dds",
		L"tex_02.dds",
		L"tex_03.dds",
		L"tex_04.dds",
		L"tex_05.dds"
	};
	const WCHAR* wchNormalTexFileNameList[6] =
	{
		L"tex_00_N.dds",
		L"tex_01_N.dds",
		L"tex_02_N.dds",
		L"tex_03_N.dds",
		L"tex_04_N.dds",
		L"tex_05_N.dds"
	};

	DWORD dwVertexCountPerTriGroup = dwVertexCount / PLANE_COUNT;
	DWORD dwIndexCountPerTriGroup = dwIndexCount / PLANE_COUNT;
	DWORD dwTriCountPerTriGroup = dwIndexCountPerTriGroup / 3;

	// Set meshes to the BasicMeshObject
	m_pRenderer->BeginCreateMesh(m_pMeshObj, pVertexList, dwVertexCount, PLANE_COUNT);	// 박스의 6면-1면당 삼각형 2개-인덱스 6개
	for (DWORD i = 0; i < PLANE_COUNT; i++)
	{
		DWORD dwTexIndex = rand() % 6;
		m_pRenderer->InsertTriGroup(m_pMeshObj, pIndexList + i * dwIndexCountPerTriGroup, dwTriCountPerTriGroup, wchDiffuseTexFileNameList[dwTexIndex], wchNormalTexFileNameList[dwTexIndex], MaterialType::Default, FALSE);
	}
	m_pRenderer->EndCreateMesh(m_pMeshObj);

	// delete vertices and indices
	DeleteGridBox(&pVertexList, &pIndexList);

	if (m_pMeshObj)
	{
		m_pBlasHandle = m_pRenderer->CreateBLAS(m_pMeshObj, TRUE);
	}
	m_ObjectType = GAME_OBJECT_TYPE_GRID_BOX;
	m_bDeformable = TRUE;
	return m_pMeshObj;
}

void* CGameObject::CreateBottomMeshObject()
{
	// create bottom mesh
	// create vertices and indices
	DWORD	pIndexList[6] = {};
	BasicVertex pVertexList[4] = {};
	
	CreateBottomMesh(pVertexList, 4, pIndexList, 6, 20.0f, -1.0f);
	
	// create BasicMeshObject from Renderer
	m_pMeshObj = m_pRenderer->CreateBasicMeshObject();

	// L"tilemap_008_N.dds"
	// Set meshes to the BasicMeshObject
	m_pRenderer->BeginCreateMesh(m_pMeshObj, pVertexList, 4, 1);	// 바닥면 점 4개, 면 면그룹 1개
	m_pRenderer->InsertTriGroup(m_pMeshObj, pIndexList, 2, L"tilemap_008.dds", L"tilemap_008_N.dds", MaterialType::Default, FALSE);
	m_pRenderer->EndCreateMesh(m_pMeshObj);

	if (m_pMeshObj)
	{
		m_pBlasHandle = m_pRenderer->CreateBLAS(m_pMeshObj, FALSE);
	}
	m_ObjectType = GAME_OBJECT_TYPE_BOTTOM;
	return m_pMeshObj;
}
void* CGameObject::CreateWaterMeshObject()
{
	// create bottom mesh
	// create vertices and indices
	DWORD	pIndexList[6] = {};
	BasicVertex pVertexList[4] = {};
	
	CreateBottomMesh(pVertexList, 4, pIndexList, 6, 10.0f, 0.0f);
	
	// create BasicMeshObject from Renderer
	m_pMeshObj = m_pRenderer->CreateBasicMeshObject();

	// L"tilemap_008_N.dds"
	// Set meshes to the BasicMeshObject
	m_pRenderer->BeginCreateMesh(m_pMeshObj, pVertexList, 4, 1);	// 바닥면 점 4개, 면 면그룹 1개
	m_pRenderer->InsertTriGroup(m_pMeshObj, pIndexList, 2, L"Wat_S_Mo_000.dds", nullptr, MaterialType::Glass, FALSE);
	m_pRenderer->EndCreateMesh(m_pMeshObj);

	if (m_pMeshObj)
	{
		m_pBlasHandle = m_pRenderer->CreateBLAS(m_pMeshObj, FALSE);
	}
	m_ObjectType = GAME_OBJECT_TYPE_WATER;
	return m_pMeshObj;
}
void* CGameObject::CreateWallMeshObject()
{
	// create bottom mesh
	// create vertices and indices
	DWORD	pIndexList[6] = {};
	BasicVertex pVertexList[4] = {};
	
	CreateWallMesh(pVertexList, 4, pIndexList, 6, 10.0f, -1.0f);
	
	// create BasicMeshObject from Renderer
	m_pMeshObj = m_pRenderer->CreateBasicMeshObject();

	// L"tilemap_008_N.dds"
	// Set meshes to the BasicMeshObject
	m_pRenderer->BeginCreateMesh(m_pMeshObj, pVertexList, 4, 1);	// 바닥면 점 4개, 면 면그룹 1개
	m_pRenderer->InsertTriGroup(m_pMeshObj, pIndexList, 2, L"tilemap_008_alpha.dds", nullptr, MaterialType::Matte, TRUE);
	m_pRenderer->EndCreateMesh(m_pMeshObj);

	if (m_pMeshObj)
	{
		m_pBlasHandle = m_pRenderer->CreateBLAS(m_pMeshObj, FALSE);
	}
	m_ObjectType = GAME_OBJECT_TYPE_WALL;
	return m_pMeshObj;
}
void* CGameObject::CreateQuadMesh()
{
	m_pMeshObj = m_pRenderer->CreateBasicMeshObject();

	// Set meshes to the BasicMeshObject
	// normal(0,0,-1), tangent(1,0,0), binormal(0,1,0)
	BasicVertex pVertexList[] =
	{
		{ { -0.25f, 0.25f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },
		{ { 0.25f, 0.25f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
		{ { 0.25f, -0.25f, 0.0f }, {0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
		{ { -0.25f, -0.25f, 0.0f }, {0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
	};

	DWORD pIndexList[] =
	{
		0, 1, 2,
		0, 2, 3
	};

	m_pRenderer->BeginCreateMesh(m_pMeshObj, pVertexList, (DWORD)_countof(pVertexList), 1);	
	m_pRenderer->InsertTriGroup(m_pMeshObj, pIndexList, 2, L"tex_06.dds", nullptr, MaterialType::Default, FALSE);
	m_pRenderer->EndCreateMesh(m_pMeshObj);

	if (m_pMeshObj)
	{
		m_pBlasHandle = m_pRenderer->CreateBLAS(m_pMeshObj, FALSE);
	}
	m_ObjectType = GAME_OBJECT_TYPE_QUAD;
	return m_pMeshObj;
}
void CGameObject::Cleanup()
{
	if (m_pMeshObj)
	{
		if (m_pBlasHandle)
		{
			m_pRenderer->DeleteBLAS(m_pMeshObj, m_pBlasHandle);
			m_pBlasHandle = nullptr;
		}
		m_pRenderer->DeleteBasicMeshObject(m_pMeshObj);
		m_pMeshObj = nullptr;
	}
}
CGameObject::~CGameObject()
{
	Cleanup();
}
