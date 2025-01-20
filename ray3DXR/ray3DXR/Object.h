#pragma once

class Application;
class BottomLevelAccelerationStructureGeometry;
class ConstantBuffer;
class Material;

__declspec(align(16)) struct ObjectCommonCB
{
	UINT MaterialID;
	BOOL bIsVertexAnimated;

	float dummy[2];
};

class Object
{
public:
	Object() = default;
	virtual ~Object() { Cleanup(); }

	bool Initialize(const WCHAR* pszNAME, UINT sizePerVertex, UINT numVertex, const void* pVERTICES, UINT sizePerIndex, UINT numIndex, const void* pINDICES);
	bool Cleanup();

protected:
	BottomLevelAccelerationStructureGeometry* m_pGeometryInfo = nullptr;
	ConstantBuffer* m_pConstantBuffer = nullptr;
	UINT m_MaterialID = UINT_MAX;

	// DO NOT Release directly.
	Application* m_pApp = nullptr;
};

class Quad final : public Object
{
public:
	Quad() = default;
	~Quad() { --ms_QuadCount; }

	bool Initialize(Application* pApp, float width, float height, UINT materialID, const DirectX::XMFLOAT4X4 TRANSFORM);

private:
	static UINT ms_QuadCount;
};

class Box final : public Object
{
public:
	Box() = default;
	~Box() { --ms_BoxCount; }

	bool Initialize(Application* pApp, float width, float height, float depth, UINT materialID, const DirectX::XMFLOAT4X4 TRANSFORM);

private:
	static UINT ms_BoxCount;
};

class Sphere final : public Object
{
public:
	Sphere() = default;
	~Sphere() { --ms_SphereCount; }

	bool Initialize(Application* pApp, UINT materialID, const DirectX::XMFLOAT4X4 TRANSFORM);

private:
	static UINT ms_SphereCount;

	float m_Radius = 0.0f;
};
