#pragma once

class ConstantBuffer;
class Texture;

enum MaterialType
{
	MaterialType_None = 0,
	MaterialType_Labmertian,
	MaterialType_Metal,
	MaterialType_Dieletric,
	MaterialType_DiffuseLight,
	MaterialType_Count
};

__declspec(align(16)) struct MaterialConstantsData
{
	int MatType;
	DirectX::XMFLOAT3 Color;
	float Fuzz;
	float RefractionIndex;

	float dummy[2];
};

interface Material
{
	virtual bool Cleanup() = 0;

	ConstantBuffer* m_pConstantBuffer;
};

class Lambertian final : public Material
{
public:
	Lambertian() = default;
	~Lambertian() { Cleanup(); }

	bool Initialize(ID3D12Device5* pDevice, const DirectX::XMFLOAT3& COLOR);
	bool Cleanup() override;

private:
	Texture* m_pTexture = nullptr;
};

class Metal final : public Material
{
public:
	Metal() = default;
	~Metal() { Cleanup(); }

	bool Initialize(ID3D12Device5* pDevice, const DirectX::XMFLOAT3& COLOR, const float FUZZ);
	bool Cleanup() override;

private:
	DirectX::XMFLOAT3 m_Albedo;
	float m_Fuzz;
};

class Dielectric final : public Material
{
public:
	Dielectric() = default;
	~Dielectric() { Cleanup(); }

	bool Initialize(ID3D12Device5* pDevice, const float REFRACTION_INDEX);
	bool Cleanup() override;

private:
	float m_RefractionIndex;
};

class DiffuseLight final : public Material
{
public:
	DiffuseLight() = default;
	~DiffuseLight() { Cleanup(); }

	bool Initialize(ID3D12Device5* pDevice, const DirectX::XMFLOAT3& COLOR);
	bool Cleanup() override;

private:
	Texture* m_pTexture = nullptr;
};
