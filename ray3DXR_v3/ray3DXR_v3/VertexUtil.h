#pragma once


DWORD CreateBoxMesh(BasicVertex** ppOutVertexList, DWORD* pOutIndexList, DWORD dwMaxBufferCount, float fHalfBoxLen);	// alloc, must free()
void DeleteBoxMesh(BasicVertex* pVertexList);	// free

DWORD CreateBottomMesh(BasicVertex* pOutVertexList, DWORD dwMaxVertexCount, DWORD* pOutIndexList, DWORD dwMaxIndexCount, float fHalfWidthDepth, float fHeight); // not needed free()
DWORD CreateWallMesh(BasicVertex* pOutVertexList, DWORD dwMaxVertexCount, DWORD* pOutIndexList, DWORD dwMaxIndexCount, float fHalfWidthDepth, float fHeight); // not needed free()

DWORD CreateGridPerPlane(BasicVertex* pOutVertexList, DWORD dwMaxVertexBufferCount, DWORD* pOutIndexList, DWORD dwMaxIndexBufferCount, const XMFLOAT3* pStart, const XMFLOAT3* pEnd,
						 DWORD dwStartVertexIndex,
						 int iWidth, int iHeight,
						 int u_index, int v_index,
						 const XMFLOAT4* pColor,
						 DWORD* pdwOutIndexCount);

DWORD CreateGridBox(BasicVertex** ppOutVertexList, DWORD** ppOutIndexList, DWORD* pdwOutIndexCount, int iWidth, int iHeight, float fHalfBoxLen);
void DeleteGridBox(BasicVertex** ppInOutVertexList, DWORD** ppInOutIndexList);

void CreateSphereMesh(float radius, UINT sliceCount, UINT stackCount, BasicVertex** ppOutVertexList, UINT* pVertexCount, DWORD** ppOutIndexList, UINT* pIndexCount, DirectX::XMFLOAT4* pColor, int materialID);
void DeleteSphereMesh(BasicVertex** ppOutVertexLis, DWORD** ppOutIndexList);
