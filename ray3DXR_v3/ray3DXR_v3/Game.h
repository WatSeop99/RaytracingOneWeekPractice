#pragma once

class CD3D12Renderer;
class CGameObject;

class CGame
{
	CD3D12Renderer*	m_pRenderer = nullptr;
	HWND	m_hWnd = nullptr;
	void* m_pSpriteObjCommon = nullptr;

	BYTE* m_pTextImage = nullptr;
	UINT m_TextImageWidth = 0;
	UINT m_TextImageHeight = 0;
	void* m_pTextTexTexHandle = nullptr;
	void* m_pFontObj = nullptr;

	BOOL	m_bShiftKeyDown = FALSE;
	DWORD	m_dwTicksPerFrame = 16;
	float m_CamOffsetX = 0.0f;
	float m_CamOffsetY = 0.0f;
	float m_CamOffsetZ = 0.0f;

	BOOL	m_bCamRotMode = FALSE;
	int		m_iCurMouseX = 0;
	int		m_iCurMouseY = 0;
	int		m_iPrvMouseX = 0;
	int		m_iPrvMouseY = 0;
	int		m_iMouseX_RButtonPressed = 0;
	int		m_iMouseY_RButtonPressed = 0;
	BOOL	m_bMouseLButtonDown = FALSE;
	BOOL	m_bMouseMButtonDown = FALSE;
	BOOL	m_bMouseRButtonDown = FALSE;


	// Game Objects
	SORT_LINK*	m_pGameObjLinkHead = nullptr;
	SORT_LINK*	m_pGameObjLinkTail = nullptr;
	void*	m_pSkyCubeTex = nullptr;
	
	
	ULONGLONG m_PrvFrameCheckTick = 0;
	ULONGLONG m_PrvUpdateTick = 0;
	DWORD	m_FrameCount = 0;
	DWORD	m_FPS = 0;
	WCHAR	m_wchText[64] = {};

	void	Render();
	CGameObject* CreateGameObjectAsBox();
	CGameObject* CreateGameObjectAsGridBox();
	CGameObject* CreateGameObjectAsBottom();
	CGameObject* CreateGameObjectAsWater();
	CGameObject* CreateGameObjectAsWall();
	void	DeleteGameObject(CGameObject* pGameObj);
	void	DeleteAllGameObjects();

	

	void	Cleanup();
public:
	BOOL	Initialize(HWND hWnd, BOOL bEnableDebugLayer, BOOL bEnableGBV, BOOL bDebugShader);
	void	Run();
	BOOL	Update(ULONGLONG CurTick);
	void	OnKeyDown(UINT nChar, UINT uiScanCode);
	void	OnKeyUp(UINT nChar, UINT uiScanCode);
	void	OnMouseLButtonDown(int x, int y, UINT nFlags);
	void	OnMouseLButtonUp(int x, int y, UINT nFlags);
	void	OnMouseRButtonDown(int x, int y, UINT nFlags);
	void	OnMouseRButtonUp(int x, int y, UINT nFlags);
	void	OnMouseMButtonDown(int x, int y, UINT nFlags);
	void	OnMouseMButtonUp(int x, int y, UINT nFlags);
	void	OnMouseMove(int x, int y, UINT nFlags);
	void	OnMouseWheel(int x, int y, int iWheel);
	void	OnMouseHWheel(int x, int y, int iWheel);
	BOOL	UpdateWindowSize(DWORD dwBackBufferWidth, DWORD dwBackBufferHeight);

	CD3D12Renderer* INL_GetRenderer() const { return m_pRenderer; }

	CGame();
	~CGame();
};