#pragma once

class Texture;

interface Material
{
	virtual bool Initialize() = 0;
	virtual bool Cleanup() = 0;

	// ConstantBuffer* m_pConstantBuffer;
};

class Lambertian : public Material
{
public:
	Lambertian() = default;
	~Lambertian() = default;

	bool Initialize() override;
	bool Cleanup() override;

private:
	Texture* m_pTexture = nullptr;
};

class Metal : public Material
{
public:
	Metal() = default;
	~Metal() = default;

	bool Initialize() override;
	bool Cleanup() override;

private:
	DirectX::XMFLOAT3 m_Albedo;
	double m_Fuzz;
};

class Dielectric : public Material
{
public:
	Dielectric() = default;
	~Dielectric() = default;

	bool Initialize() override;
	bool Cleanup() override;

private:
	double m_RefractionIndex;
};

class DiffuseLight : public Material
{
public:
	DiffuseLight() = default;
	~DiffuseLight() = default;

	bool Initialize() override;
	bool Cleanup() override;

private:
	Texture* m_pTexture = nullptr;
};
