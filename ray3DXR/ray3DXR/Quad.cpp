#include "framework.h"
#include "Material.h"
#include "Vertex.h"
#include "Quad.h"

bool Quad::Initialize(const DirectX::XMFLOAT3& Q, const DirectX::XMFLOAT3 U, const DirectX::XMFLOAT3& V, Material* pMat)
{
	_ASSERT(pMat);

	m_Q = Q;
	m_U = U;
	m_V = V;
	m_pMaterial = pMat;

	DirectX::XMFLOAT3 temp;

	DirectX::XMVECTOR n = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_U), DirectX::XMLoadFloat3(&m_V));
	
	DirectX::XMStoreFloat3(&m_Normal, DirectX::XMVector3Normalize(n));
	
	DirectX::XMStoreFloat3(&temp, DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&m_Normal), DirectX::XMLoadFloat3(&m_Q)));
	m_D = temp.x;

	DirectX::XMStoreFloat3(&m_W, DirectX::XMVectorDivide(n, DirectX::XMVector3Dot(n, n)));
	
	DirectX::XMStoreFloat3(&temp, DirectX::XMVector3Length(n));
	m_Area = temp.x;

	Vertex vertices[4];
	UINT indices[6] = {};

	vertices[0] = { m_Q, m_Normal };
	DirectX::XMStoreFloat3(&temp, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_Q), DirectX::XMLoadFloat3(&m_U)));
	vertices[1] = { temp, m_Normal };
	DirectX::XMStoreFloat3(&temp, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_Q), DirectX::XMLoadFloat3(&m_V)));
	vertices[2] = { temp, m_Normal };
	DirectX::XMStoreFloat3(&temp, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_Q), DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_U), DirectX::XMLoadFloat3(&m_V))));
	vertices[3] = { temp, m_Normal };

	m_pVertexBuffer;
	m_pIndexBuffer;

	return true;
}
