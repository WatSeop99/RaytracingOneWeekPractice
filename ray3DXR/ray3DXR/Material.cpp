#include "framework.h"
#include "Application.h"
#include "GPUResource.h"
#include "Texture.h"
#include "Material.h"

bool Material::Initialize(Application* pApp)
{
	_ASSERT(pApp);

	m_MaterialType = MaterialType_Labmertian;

	m_pConstantBuffer = new ConstantBuffer;
	if (!m_pConstantBuffer)
	{
		return false;
	}

	return m_pConstantBuffer->Initialize(pApp, sizeof(MaterialConstantsData), nullptr);
}

bool Material::Cleanup()
{
	if (m_pConstantBuffer)
	{
		delete m_pConstantBuffer;
		m_pConstantBuffer = nullptr;
	}

	return true;
}

void Material::SetMaterialID(UINT materialID)
{
	_ASSERT(m_pConstantBuffer);
	_ASSERT(materialID != UINT_MAX);

	m_MaterialID = materialID;

	MaterialConstantsData* pSystemMem = (MaterialConstantsData*)m_pConstantBuffer->GetDataMem();
	pSystemMem->MaterialID = materialID;

	m_pConstantBuffer->Upload();
}

bool Lambertian::Initialize(Application* pApp, const DirectX::XMFLOAT3& COLOR)
{
	_ASSERT(pApp);

	m_MaterialType = MaterialType_Labmertian;

	m_pTexture = new SolidColor(COLOR);
	m_pConstantBuffer = new ConstantBuffer;
	if (!m_pTexture || !m_pConstantBuffer)
	{
		return false;
	}

	return m_pConstantBuffer->Initialize(pApp, sizeof(MaterialConstantsData), nullptr);
}

bool Lambertian::Cleanup()
{
	if (m_pTexture)
	{
		delete m_pTexture;
		m_pTexture = nullptr;
	}

	return true;
}

bool Metal::Initialize(Application* pApp, const DirectX::XMFLOAT3& COLOR, const float FUZZ)
{
	_ASSERT(pApp);

	m_MaterialType = MaterialType_Metal;
	m_Albedo = COLOR;
	m_Fuzz = FUZZ;

	m_pConstantBuffer = new ConstantBuffer;
	if (!m_pConstantBuffer)
	{
		return false;
	}

	return m_pConstantBuffer->Initialize(pApp, sizeof(MaterialConstantsData), nullptr);
};

bool Dielectric::Initialize(Application* pApp, const float REFRACTION_INDEX)
{
	_ASSERT(pApp);

	m_MaterialType = MaterialType_Dielectric;
	m_RefractionIndex = REFRACTION_INDEX;

	m_pConstantBuffer = new ConstantBuffer;
	if (!m_pConstantBuffer)
	{
		return false;
	}

	return m_pConstantBuffer->Initialize(pApp, sizeof(MaterialConstantsData), nullptr);
}

bool DiffuseLight::Initialize(Application* pApp, const DirectX::XMFLOAT3& COLOR)
{
	_ASSERT(pApp);

	m_MaterialType = MaterialType_DiffuseLight;

	m_pTexture = new SolidColor(COLOR);
	m_pConstantBuffer = new ConstantBuffer;
	if (!m_pTexture || !m_pConstantBuffer)
	{
		return false;
	}

	return m_pConstantBuffer->Initialize(pApp, sizeof(MaterialConstantsData), nullptr);
}

bool DiffuseLight::Cleanup()
{
	if (m_pTexture)
	{
		delete m_pTexture;
		m_pTexture = nullptr;
	}

	return true;
}

bool MaterialManager::Initialize(Application* pApp, SIZE_T maxCount)
{
	_ASSERT(pApp);
	_ASSERT(maxCount > 0);
	_ASSERT(!m_pMaterialBuffer);

	m_pApp = pApp;
	m_MaxCount = maxCount;

	m_pMaterialBuffer = new StructuredBuffer;
	if (!m_pMaterialBuffer->Initialize(pApp, sizeof(MaterialData), maxCount, nullptr))
	{
		return false;
	}

	return true;
}

bool MaterialManager::Cleanup()
{
	if (m_pMaterialBuffer)
	{
		delete m_pMaterialBuffer;
		m_pMaterialBuffer = nullptr;
	}

	for (SIZE_T i = 0, size = m_Materials.size(); i < size; ++i)
	{
		delete m_Materials[i];
	}
	m_Materials.clear();

	m_MaxCount = 0;
	m_pApp = nullptr;

	return true;
}

bool MaterialManager::AddMaterial(Material* pMaterial)
{
	_ASSERT(pMaterial);
	_ASSERT(m_pMaterialBuffer);
	_ASSERT(m_MaxCount > 0);

	if (m_Materials.size() + 1 >= m_MaxCount)
	{
		return false;
	}

	SIZE_T materialID = m_Materials.size();
	pMaterial->SetMaterialID(materialID);
	m_Materials.push_back(pMaterial);

	MaterialData* pSysMem = (MaterialData*)m_pMaterialBuffer->GetDataMem();
	(pSysMem + materialID)->MatType = pMaterial->GetMaterialType();
	switch ((pSysMem + materialID)->MatType)
	{
		case MaterialType_Labmertian:
			(pSysMem + materialID)->Color = ((Lambertian*)pMaterial)->GetTexture()->Value();
			break;

		case MaterialType_Metal:
			(pSysMem + materialID)->Color = ((Metal*)pMaterial)->GetColor();
			(pSysMem + materialID)->Fuzz = ((Metal*)pMaterial)->GetFuzz();
			break;

		case MaterialType_Dielectric:
			(pSysMem + materialID)->RefractionIndex = ((Dielectric*)pMaterial)->GetRefractionIndex();
			break;

		case MaterialType_DiffuseLight:
			(pSysMem + materialID)->Color = ((DiffuseLight*)pMaterial)->GetTexture()->Value();
			break;

		case MaterialType_None:
		default:
			__debugbreak();
			break;
	}

	return m_pMaterialBuffer->Upload();
}