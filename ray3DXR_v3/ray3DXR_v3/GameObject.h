#pragma once

#include "LinkedList.h"

enum GAME_OBJECT_ACTION_TYPE
{
	GAME_OBJECT_ACTION_TYPE_NONE,
	GAME_OBJECT_ACTION_TYPE_MOVE_X,
	GAME_OBJECT_ACTION_TYPE_MOVE_Y,
	GAME_OBJECT_ACTION_TYPE_MOVE_Z,
	GAME_OBJECT_ACTION_TYPE_ROT_Y,
	GAME_OBJECT_ACTION_TYPE_SCALE_X,
	GAME_OBJECT_ACTION_TYPE_SCALE_Y,
	GAME_OBJECT_ACTION_TYPE_SCALE_Z,
	GAME_OBJECT_ACTION_TYPE_COUNT
};

enum GAME_OBJECT_TYPE
{
	GAME_OBJECT_TYPE_BOX,
	GAME_OBJECT_TYPE_GRID_BOX,
	GAME_OBJECT_TYPE_WALL,
	GAME_OBJECT_TYPE_WATER,
	GAME_OBJECT_TYPE_QUAD,
	GAME_OBJECT_TYPE_BOTTOM,
	GAME_OBJECT_TYPE_COUNT
};


class CGame;
class CD3D12Renderer;
class CGameObject
{
	
	CGame* m_pGame = nullptr;
	CD3D12Renderer* m_pRenderer = nullptr;
	void* m_pMeshObj = nullptr;
	void* m_pBlasHandle = nullptr;
	
	XMVECTOR m_Scale = {1.0f, 1.0f, 1.0f, 0.0f};
	XMVECTOR m_Pos = {};
	float m_fRotY = 0.0f;

	XMMATRIX m_matScale = {};
	XMMATRIX m_matRot = {};
	XMMATRIX m_matTrans = {};
	XMMATRIX m_matWorld = {};
	
	float	m_fHalfLen = 0.0;
	BOOL	m_bUpdateTransform = FALSE;
	GAME_OBJECT_ACTION_TYPE	m_ActionType = GAME_OBJECT_ACTION_TYPE_NONE;
	GAME_OBJECT_TYPE m_ObjectType = GAME_OBJECT_TYPE_BOX;
	BOOL m_bDeformable = FALSE;
	XMFLOAT3 m_MovedOffset = {};
	float	m_fMoveSign = 1.0f;
	XMFLOAT3 m_ScaledOffset = {};
	float	m_fScaleSign = 1.0f;
	
	void	UpdateTransform();
	void	Cleanup();
public:
	SORT_LINK	m_LinkInGame;
	
	BOOL	Initialize(CGame* pGame, GAME_OBJECT_ACTION_TYPE actionType);
	void*	CreateBoxMeshObject();
	void*	CreateGridBoxMeshObject();
	void*	CreateQuadMesh();
	void*	CreateBottomMeshObject();
	void*	CreateWaterMeshObject();
	void*	CreateWallMeshObject();
	void	GetPosition(float* pfOutX, float* pfOutY, float* pfOutZ);
	void	GetScale(float* pfOutX, float* pfOutY, float* pfOutZ);
	float	GetRotationY();
	void	SetPosition(float x, float y, float z);
	void	SetScale(float x, float y, float z);
	void	SetRotationY(float fRotY);
	void	Run(DWORD dwFrameCount);
	void	Render();
	CGameObject();
	~CGameObject();
};