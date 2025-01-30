#pragma once

class Application;
class ConstantBuffer;
class Texture;
class StructuredBuffer;

enum MaterialType
{
	MaterialType_None = 0,
	MaterialType_Labmertian,
	MaterialType_Metal,
	MaterialType_Dielectric,
	MaterialType_DiffuseLight,
	MaterialType_Count
};

__declspec(align(16)) struct MaterialData
{
	int MatType;
	DirectX::XMFLOAT3 Color;
	float Fuzz;
	float RefractionIndex;

	float dummy[2];
};
__declspec(align(16)) struct MaterialConstantsData
{
	UINT MaterialID;
	
	float dummy[3];
};

class Material
{
public:
	Material() = default;
	virtual ~Material() { Cleanup(); }

	bool Initialize(Application* pApp);
	virtual bool Cleanup();

	virtual void SetMaterialID(UINT materialID);

	inline ConstantBuffer* GetConstantBuffer() { return m_pConstantBuffer; }
	inline MaterialType GetMaterialType() { return m_MaterialType; }
	inline UINT GetMaterialID() { return m_MaterialID; }

protected:
	ConstantBuffer* m_pConstantBuffer;
	MaterialType m_MaterialType = MaterialType_None;
	UINT m_MaterialID = UINT_MAX;
};

class Lambertian final : public Material
{
public:
	Lambertian() = default;
	~Lambertian() { Cleanup(); }

	bool Initialize(Application* pApp, const DirectX::XMFLOAT3& COLOR);
	bool Cleanup() override;

	inline Texture* GetTexture() { return m_pTexture; }

private:
	Texture* m_pTexture = nullptr;
};

class Metal final : public Material
{
public:
	Metal() = default;
	~Metal() = default;

	bool Initialize(Application* pApp, const DirectX::XMFLOAT3& COLOR, const float FUZZ);

	inline DirectX::XMFLOAT3 GetColor() { return m_Albedo; }
	inline float GetFuzz() { return m_Fuzz; }

private:
	DirectX::XMFLOAT3 m_Albedo;
	float m_Fuzz;
};

class Dielectric final : public Material
{
public:
	Dielectric() = default;
	~Dielectric() = default;

	bool Initialize(Application* pApp, const float REFRACTION_INDEX);

	inline float GetRefractionIndex() { return m_RefractionIndex; }

private:
	float m_RefractionIndex;
};

class DiffuseLight final : public Material
{
public:
	DiffuseLight() = default;
	~DiffuseLight() { Cleanup(); }

	bool Initialize(Application* pApp, const DirectX::XMFLOAT3& COLOR);
	bool Cleanup() override;

	inline Texture* GetTexture() { return m_pTexture; }

private:
	Texture* m_pTexture = nullptr;
};

class MaterialManager
{
public:
	MaterialManager() = default;
	~MaterialManager() { Cleanup(); }

	bool Initialize(Application* pApp, SIZE_T maxCount);
	bool Cleanup();

	bool AddMaterial(Material* pMaterial);

	inline StructuredBuffer* GetMaterialBuffer() { return m_pMaterialBuffer; }

private:
	StructuredBuffer* m_pMaterialBuffer = nullptr;
	std::vector<Material*> m_Materials;
	SIZE_T m_MaxCount = 0;

	// DO NOT Release directly.
	Application* m_pApp = nullptr;
};
