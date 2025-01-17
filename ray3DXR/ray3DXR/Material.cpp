#include "framework.h"
#include "GPUResource.h"
#include "Texture.h"
#include "Material.h"

bool Lambertian::Initialize(ID3D12Device5* pDevice, const DirectX::XMFLOAT3& COLOR)
{
	_ASSERT(pDevice);

	m_pTexture = new SolidColor(COLOR);
	m_pConstantBuffer = new ConstantBuffer;

	MaterialConstantsData initData = { 0, };
	initData.MatType = MaterialType_Labmertian;
	initData.Color = COLOR;

	return m_pConstantBuffer->Initialize(pDevice, sizeof(MaterialConstantsData), &initData);
}

bool Lambertian::Cleanup()
{
	if (m_pTexture)
	{
		delete m_pTexture;
		m_pTexture = nullptr;
	}
	if (m_pConstantBuffer)
	{
		delete m_pConstantBuffer;
		m_pConstantBuffer = nullptr;
	}

	return true;
}

bool Metal::Initialize(ID3D12Device5* pDevice, const DirectX::XMFLOAT3& COLOR, const float FUZZ)
{
	_ASSERT(pDevice);

	m_Albedo = COLOR;
	m_Fuzz = FUZZ;

	m_pConstantBuffer = new ConstantBuffer;

	MaterialConstantsData initData = { 0, };
	initData.MatType = MaterialType_Metal;
	initData.Color = m_Albedo;
	initData.Fuzz = m_Fuzz;

	return m_pConstantBuffer->Initialize(pDevice, sizeof(MaterialConstantsData), &initData);
};

bool Metal::Cleanup()
{
	if (m_pConstantBuffer)
	{
		delete m_pConstantBuffer;
		m_pConstantBuffer = nullptr;
	}

	return true;
}

bool Dielectric::Initialize(ID3D12Device5* pDevice, const float REFRACTION_INDEX)
{
	_ASSERT(pDevice);

	m_RefractionIndex = REFRACTION_INDEX;

	m_pConstantBuffer = new ConstantBuffer;

	MaterialConstantsData initData = { 0, };
	initData.MatType = MaterialType_Dieletric;
	initData.RefractionIndex = m_RefractionIndex;

	return m_pConstantBuffer->Initialize(pDevice, sizeof(MaterialConstantsData), &initData);
}

bool Dielectric::Cleanup()
{
	if (m_pConstantBuffer)
	{
		delete m_pConstantBuffer;
		m_pConstantBuffer = nullptr;
	}

	return true;
}

bool DiffuseLight::Initialize(ID3D12Device5* pDevice, const DirectX::XMFLOAT3& COLOR)
{
	_ASSERT(pDevice);

	m_pTexture = new SolidColor(COLOR);
	m_pConstantBuffer = new ConstantBuffer;

	MaterialConstantsData initData = { 0, };
	initData.MatType = MaterialType_DiffuseLight;
	initData.Color = COLOR;

	return m_pConstantBuffer->Initialize(pDevice, sizeof(MaterialConstantsData), &initData);
}

bool DiffuseLight::Cleanup()
{
	if (m_pTexture)
	{
		delete m_pTexture;
		m_pTexture = nullptr;
	}
	if (m_pConstantBuffer)
	{
		delete m_pConstantBuffer;
		m_pConstantBuffer = nullptr;
	}

	return true;
}
