#pragma once

#include "Object.h"

interface Material;

class Quad : public Object
{
public:
	Quad() = default;
	~Quad() = default;

	bool Initialize(const DirectX::XMFLOAT3& Q, const DirectX::XMFLOAT3 U, const DirectX::XMFLOAT3& V, Material* pMat);

private:
	DirectX::XMFLOAT3 m_Q;
	DirectX::XMFLOAT3 m_U;
	DirectX::XMFLOAT3 m_V;
	DirectX::XMFLOAT3 m_W;
	DirectX::XMFLOAT3 m_Normal;
	double m_D;
	double m_Area;

	// DO NOT Release directly.
	Material* m_pMaterial = nullptr;
};
