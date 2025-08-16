#include "pch.h"
#include <Windows.h>
#include <DirectXMath.h>
#include "D3D12Renderer.h"
#include "GameObject.h"
#include "Util.h"
#include "Game.h"

CGame::CGame()
{

}
BOOL CGame::Initialize(HWND hWnd, BOOL bEnableDebugLayer, BOOL bEnableGBV, BOOL bDebugShader)
{
	const DWORD BOX_OBJ_COUNT = 200;
	const DWORD GAME_OBJ_COUNT = BOX_OBJ_COUNT + 1 + 1;	// box meshes + bottom + wall 

	WCHAR wchAppPath[_MAX_PATH];
	GetCurrentDirectory(_MAX_PATH, wchAppPath);

	WCHAR wchShaderPath[_MAX_PATH];
	SetCurrentDirectory(L"./Shaders");
	GetCurrentDirectory(_MAX_PATH, wchShaderPath);

	SetCurrentDirectory(wchAppPath);

	m_pRenderer = new CD3D12Renderer;
	m_pRenderer->Initialize(hWnd, bEnableDebugLayer, bEnableGBV, bDebugShader, wchShaderPath, GAME_OBJ_COUNT);
	m_hWnd = hWnd;

	// Create Font
	m_pFontObj = m_pRenderer->CreateFontObject(L"Tahoma", 18.0f);

	// create texture for draw text
	m_TextImageWidth = 512;
	m_TextImageHeight = 256;
	m_pTextImage = (BYTE*)malloc(m_TextImageWidth * m_TextImageHeight * 4);
	m_pTextTexTexHandle = m_pRenderer->CreateDynamicTexture(m_TextImageWidth, m_TextImageHeight);
	memset(m_pTextImage, 0, m_TextImageWidth * m_TextImageHeight * 4);

	m_pSpriteObjCommon = m_pRenderer->CreateSpriteObject();


	//for (DWORD i = 0; i < BOX_OBJ_COUNT; i++)
	//{
	//	CGameObject* pGameObj = CreateGameObjectAsGridBox();
	//	if (pGameObj)
	//	{
	//		float x = (float)((rand() % 41) - 20);	// -20m - 20m 
	//		float y = 0.0f;
	//		float z = (float)((rand() % 41) - 20);	// -20m - 20m 
	//		pGameObj->SetPosition(x, y, z);
	//		float rad = (rand() % 181) * (3.1415f / 180.0f);
	//		pGameObj->SetRotationY(rad);
	//	}
	//}
	//CGameObject* pBottom = CreateGameObjectAsBottom();
	//CGameObject* pWall = CreateGameObjectAsWall();	

	enum eColorType
	{
		ColorType_Lambertian = 0,
		ColorType_Metallic,
		ColorType_Dielectric
	};
	const int SLICE_COUNT = 16;
	const int STACK_COUNT = 32;

	{
		DirectX::XMFLOAT4 color(0.5f, 0.5f, 0.5f, 1.0f);
		CGameObject* pSphere = CreateGameObjectAsSphere(1000.0f, SLICE_COUNT, 512, &color, (int)ColorType_Lambertian);
		pSphere->SetPosition(0.0f, -1000.0f, 0.0f);
	}
	{
		DirectX::XMFLOAT4 color;
		CGameObject* pSphere1 = nullptr;
		CGameObject* pSphere2 = nullptr;
		CGameObject* pSphere3 = nullptr;

		color = { 1.5f, 1.5f, 1.5f, 1.5f };
		pSphere1 = CreateGameObjectAsSphere(1.0f, SLICE_COUNT, STACK_COUNT, &color, ColorType_Dielectric);
		pSphere1->SetPosition(0.0f, 1.0f, 0.0f);

		color = { 0.4f, 0.2f, 0.1f, 0.0f };
		pSphere2 = CreateGameObjectAsSphere(1.0f, SLICE_COUNT, STACK_COUNT, &color, ColorType_Lambertian);
		pSphere2->SetPosition(-4.0f, 1.0f, 0.0f);

		color = { 0.7f, 0.6f, 0.5f, 0.0f };
		pSphere3 = CreateGameObjectAsSphere(1.0f, SLICE_COUNT, STACK_COUNT, &color, ColorType_Metallic);
		pSphere3->SetPosition(4.0f, 1.0f, 0.0f);
	}
	{
		for (int a = -11; a < 11; ++a)
		{
			for (int b = -11; b < 11; ++b)
			{
				double chooseMat = RandomDouble();
				DirectX::XMVECTOR center = { (float)a + 0.9f * RandomFloat(), 0.2f, (float)b + 0.9f * RandomFloat() };
				DirectX::XMVECTOR length = DirectX::XMVector3Length(DirectX::XMVectorSubtract(center, { 4.0f, 0.2f,0.0f }));

				if (DirectX::XMVectorGetX(length) > 0.9f)
				{
					DirectX::XMVECTOR materialParameter;
					int materialType = 0;

					if (chooseMat < 0.8)
					{
						// diffuse
						materialType = ColorType_Lambertian;

						DirectX::XMVECTOR color1;
						DirectX::XMVECTOR color2;
						RandomColor(&color1);
						RandomColor(&color2);
						materialParameter = color1 * color2;
					}
					else if (chooseMat < 0.95)
					{
						// metal
						materialType = ColorType_Metallic;

						float fuzz = RandomFloat(0.0f, 0.5f);
						materialParameter =
						{
							RandomFloat(0.5f, 1.0f),
							RandomFloat(0.5f, 1.0f),
							RandomFloat(0.5f, 1.0f),
							fuzz
						};
					}
					else
					{
						// glass
						materialType = ColorType_Dielectric;
						materialParameter = { 1.5f, 1.5f, 1.5f, 1.5f };
					}

					DirectX::XMFLOAT4 color;
					DirectX::XMStoreFloat4(&color, materialParameter);
					CGameObject* pSphere = CreateGameObjectAsSphere(0.2f, SLICE_COUNT, STACK_COUNT, &color, materialType);
					
					DirectX::XMFLOAT3 pos;
					DirectX::XMStoreFloat3(&pos, center);
					pSphere->SetPosition(pos.x, pos.y, pos.z);
				}
			}
		}
	}

	m_pSkyCubeTex = m_pRenderer->CreateTextureFromFile(L"skycube.dds");
	if (m_pSkyCubeTex)
	{
		m_pRenderer->SetSkyCubeMap(m_pSkyCubeTex);
	}

	return TRUE;
}

CGameObject* CGame::CreateGameObjectAsBox()
{
	// meshobject를 공용으로 쓰도록 한다.
	int iActionCount = (GAME_OBJECT_ACTION_TYPE_SCALE_Z - GAME_OBJECT_ACTION_TYPE_NONE) + 1;
	GAME_OBJECT_ACTION_TYPE actionType = (GAME_OBJECT_ACTION_TYPE)((rand() % iActionCount) + GAME_OBJECT_ACTION_TYPE_NONE);

	CGameObject* pGameObj = new CGameObject;
	pGameObj->Initialize(this, actionType);
	pGameObj->CreateBoxMeshObject();
	LinkToLinkedListFIFO(&m_pGameObjLinkHead, &m_pGameObjLinkTail, &pGameObj->m_LinkInGame);

	return pGameObj;
}
CGameObject* CGame::CreateGameObjectAsGridBox()
{
	int iActionCount = (GAME_OBJECT_ACTION_TYPE_SCALE_Z - GAME_OBJECT_ACTION_TYPE_NONE) + 1;
	GAME_OBJECT_ACTION_TYPE actionType = (GAME_OBJECT_ACTION_TYPE)((rand() % iActionCount) + GAME_OBJECT_ACTION_TYPE_NONE);

	CGameObject* pGameObj = new CGameObject;
	pGameObj->Initialize(this, actionType);
	pGameObj->CreateGridBoxMeshObject();
	LinkToLinkedListFIFO(&m_pGameObjLinkHead, &m_pGameObjLinkTail, &pGameObj->m_LinkInGame);
	return pGameObj;
}
CGameObject* CGame::CreateGameObjectAsBottom()
{
	// meshobject를 공용으로 쓰도록 한다.

	CGameObject* pGameObj = new CGameObject;
	pGameObj->Initialize(this, GAME_OBJECT_ACTION_TYPE_NONE);
	pGameObj->CreateBottomMeshObject();
	LinkToLinkedListFIFO(&m_pGameObjLinkHead, &m_pGameObjLinkTail, &pGameObj->m_LinkInGame);

	return pGameObj;
}
CGameObject* CGame::CreateGameObjectAsWater()
{
	// meshobject를 공용으로 쓰도록 한다.

	CGameObject* pGameObj = new CGameObject;
	pGameObj->Initialize(this, GAME_OBJECT_ACTION_TYPE_NONE);
	pGameObj->CreateWaterMeshObject();
	LinkToLinkedListFIFO(&m_pGameObjLinkHead, &m_pGameObjLinkTail, &pGameObj->m_LinkInGame);

	return pGameObj;
}
CGameObject* CGame::CreateGameObjectAsWall()
{
	// meshobject를 공용으로 쓰도록 한다.

	CGameObject* pGameObj = new CGameObject;
	pGameObj->Initialize(this, GAME_OBJECT_ACTION_TYPE_NONE);
	pGameObj->CreateWallMeshObject();
	LinkToLinkedListFIFO(&m_pGameObjLinkHead, &m_pGameObjLinkTail, &pGameObj->m_LinkInGame);

	return pGameObj;
}
CGameObject* CGame::CreateGameObjectAsSphere(float radius, UINT sliceCount, UINT stackCount, DirectX::XMFLOAT4* pColor, int materialID)
{
	CGameObject* pGameObj = new CGameObject;
	pGameObj->Initialize(this, GAME_OBJECT_ACTION_TYPE_NONE);
	pGameObj->CreateSphereMeshObject(radius, sliceCount, stackCount, pColor, materialID);
	LinkToLinkedListFIFO(&m_pGameObjLinkHead, &m_pGameObjLinkTail, &pGameObj->m_LinkInGame);

	return pGameObj;
}

void CGame::OnKeyDown(UINT nChar, UINT uiScanCode)
{
	switch (nChar)
	{
	case VK_SHIFT:
		m_bShiftKeyDown = TRUE;
		break;
	case 'W':
		if (m_bShiftKeyDown)
		{
			m_CamOffsetY = 0.05f;
		}
		else
		{
			m_CamOffsetZ = 0.05f;
		}
		break;
	case 'S':
		if (m_bShiftKeyDown)
		{
			m_CamOffsetY = -0.05f;
		}
		else
		{
			m_CamOffsetZ = -0.05f;
		}
		break;
	case 'A':
		m_CamOffsetX = -0.05f;
		break;
	case 'D':
		m_CamOffsetX = 0.05f;
		break;
	case 'R':
	{
		BOOL bUseDXR = m_pRenderer->IsEnabledDXR();
		bUseDXR = bUseDXR == 0;
		m_pRenderer->EnableDXR(bUseDXR);
	}
	break;
	case 'F':
	{
		if (!m_dwTicksPerFrame)
			m_dwTicksPerFrame = 16;
		else
			m_dwTicksPerFrame = 0;
	}
	break;
	}
}
void CGame::OnKeyUp(UINT nChar, UINT uiScanCode)
{
	switch (nChar)
	{
	case VK_SHIFT:
		m_bShiftKeyDown = FALSE;
		break;
	case 'W':
		m_CamOffsetY = 0.0f;
		m_CamOffsetZ = 0.0f;
		break;
	case 'S':
		m_CamOffsetY = 0.0f;
		m_CamOffsetZ = 0.0f;
		break;
	case 'A':
		m_CamOffsetX = 0.0f;
		break;
	case 'D':
		m_CamOffsetX = 0.0f;
		break;
	}
}
void CGame::OnMouseLButtonDown(int x, int y, UINT nFlags)
{
	m_bMouseLButtonDown = TRUE;
}
void CGame::OnMouseLButtonUp(int x, int y, UINT nFlags)
{
	m_bMouseLButtonDown = FALSE;
}
void CGame::OnMouseRButtonDown(int x, int y, UINT nFlags)
{
	m_bCamRotMode = TRUE;
	m_iMouseX_RButtonPressed = x;
	m_iMouseY_RButtonPressed = y;

	m_bMouseRButtonDown = TRUE;
}
void CGame::OnMouseRButtonUp(int x, int y, UINT nFlags)
{
	m_bCamRotMode = FALSE;
	m_bMouseRButtonDown = FALSE;
}
void CGame::OnMouseMButtonDown(int x, int y, UINT nFlags)
{
	m_bMouseMButtonDown = TRUE;
}
void CGame::OnMouseMButtonUp(int x, int y, UINT nFlags)
{
	m_bMouseMButtonDown = FALSE;
}
void CGame::OnMouseMove(int x, int y, UINT nFlags)
{
	m_iPrvMouseX = m_iCurMouseX;
	m_iPrvMouseY = m_iCurMouseY;

	int dx = x - m_iPrvMouseX;
	int dy = y - m_iPrvMouseY;

	if (m_bCamRotMode)
	{
		if (dy != 0)
			int a = 0;

		float fYaw = (float)dx * 0.01f;
		float fPitch = (float)dy * 0.01f;
		m_pRenderer->SetCameraRot(fYaw, fPitch, 0.0f);
	}
	m_iCurMouseX = x;
	m_iCurMouseY = y;
}
void CGame::OnMouseWheel(int x, int y, int iWheel)
{}
void CGame::OnMouseHWheel(int x, int y, int iWheel)
{}
void CGame::Run()
{
	m_FrameCount++;

	// begin
	ULONGLONG CurTick = GetTickCount64();

	// game business logic
	Update(CurTick);

	Render();

	if (CurTick - m_PrvFrameCheckTick > 1000)
	{
		m_PrvFrameCheckTick = CurTick;

		WCHAR wchTxt[64];
		m_FPS = m_FrameCount;
		swprintf_s(wchTxt, L"FPS:%u", m_FPS);
		SetWindowText(m_hWnd, wchTxt);

		m_FrameCount = 0;
	}
}
BOOL CGame::Update(ULONGLONG CurTick)
{
	// Update Scene with 60FPS
	if ((DWORD)(CurTick - m_PrvUpdateTick) < m_dwTicksPerFrame)
	{
		return FALSE;
	}
	m_PrvUpdateTick = CurTick;

	// Update camra
	if (m_CamOffsetX != 0.0f || m_CamOffsetY != 0.0f || m_CamOffsetZ != 0.0f)
	{
		m_pRenderer->MoveCamera(m_CamOffsetX, m_CamOffsetY, m_CamOffsetZ);
	}

	// update game objects
	SORT_LINK* pCur = m_pGameObjLinkHead;
	while (pCur)
	{
		CGameObject* pGameObj = (CGameObject*)pCur->pItem;
		pGameObj->Run(m_FrameCount);
		pCur = pCur->pNext;
	}

	// update status text
	int iTextWidth = 0;
	int iTextHeight = 0;
	WCHAR	wchTxt[64] = {};
	DWORD	dwTxtLen = swprintf_s(wchTxt, L"Current FrameRate: %u", m_FPS);

	if (wcscmp(m_wchText, wchTxt))
	{
		// 텍스트가 변경된 경우
		memset(m_pTextImage, 0, m_TextImageWidth * m_TextImageHeight * 4);
		m_pRenderer->WriteTextToBitmap(m_pTextImage, m_TextImageWidth, m_TextImageHeight, m_TextImageWidth * 4, &iTextWidth, &iTextHeight, m_pFontObj, wchTxt, dwTxtLen);
		m_pRenderer->UpdateTextureWithImage(m_pTextTexTexHandle, m_pTextImage, m_TextImageWidth, m_TextImageHeight);
		wcscpy_s(m_wchText, wchTxt);
	}
	else
	{
		// 텍스트가 변경되지 않은 경우 - 업데이트 할 필요 없다.
		int a = 0;
	}
	return TRUE;
}
void CGame::Render()
{
	m_pRenderer->BeginRender();

	// render game objects
	SORT_LINK* pCur = m_pGameObjLinkHead;
	DWORD dwObjCount = 0;
	while (pCur)
	{
		CGameObject* pGameObj = (CGameObject*)pCur->pItem;
		pGameObj->Render();
		pCur = pCur->pNext;
		dwObjCount++;
	}
	// render dynamic texture as text
	m_pRenderer->RenderSpriteWithTex(m_pSpriteObjCommon, 512 + 5, 256 + 5 + 256 + 5, 1.0f, 1.0f, nullptr, 0.0f, m_pTextTexTexHandle);

	// end
	m_pRenderer->EndRender();

	// Present
	m_pRenderer->Present();
}
void CGame::DeleteGameObject(CGameObject* pGameObj)
{
	UnLinkFromLinkedList(&m_pGameObjLinkHead, &m_pGameObjLinkTail, &pGameObj->m_LinkInGame);
	delete pGameObj;
}
void CGame::DeleteAllGameObjects()
{
	while (m_pGameObjLinkHead)
	{
		CGameObject* pGameObj = (CGameObject*)m_pGameObjLinkHead->pItem;
		DeleteGameObject(pGameObj);
	}
}
BOOL CGame::UpdateWindowSize(DWORD dwBackBufferWidth, DWORD dwBackBufferHeight)
{
	BOOL bResult = FALSE;
	if (m_pRenderer)
	{
		bResult = m_pRenderer->UpdateWindowSize(dwBackBufferWidth, dwBackBufferHeight);
	}
	return bResult;
}
void CGame::Cleanup()
{
	DeleteAllGameObjects();

	if (m_pSkyCubeTex)
	{
		m_pRenderer->DeleteTexture(m_pSkyCubeTex);
		m_pSkyCubeTex = nullptr;
	}
	if (m_pTextImage)
	{
		free(m_pTextImage);
		m_pTextImage = nullptr;
	}
	if (m_pRenderer)
	{
		if (m_pFontObj)
		{
			m_pRenderer->DeleteFontObject(m_pFontObj);
			m_pFontObj = nullptr;
		}

		if (m_pTextTexTexHandle)
		{
			m_pRenderer->DeleteTexture(m_pTextTexTexHandle);
			m_pTextTexTexHandle = nullptr;
		}
		if (m_pSpriteObjCommon)
		{
			m_pRenderer->DeleteSpriteObject(m_pSpriteObjCommon);
			m_pSpriteObjCommon = nullptr;
		}

		delete m_pRenderer;
		m_pRenderer = nullptr;
	}
}
CGame::~CGame()
{
	Cleanup();
}


