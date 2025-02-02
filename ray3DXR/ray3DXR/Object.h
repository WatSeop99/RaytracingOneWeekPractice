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

	bool Initialize(const WCHAR* pszNAME, UINT sizePerVertex, UINT numVertex, const void* pVERTICES, UINT sizePerIndex, UINT numIndex, const void* pINDICES, bool bIsLightSource);
	bool Cleanup();

	inline BottomLevelAccelerationStructureGeometry* GetASGeometry() { return m_pGeometryInfo; }
	inline ConstantBuffer* GetConstantBuffer() { return m_pConstantBuffer; }
	inline UINT GetMaterialID() { return m_MaterialID; }

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
	~Quad() { --ms_QuadCount; --ms_LightQuadCount; }

	bool Initialize(Application* pApp, float width, float height, UINT materialID, const DirectX::XMFLOAT4X4 TRANSFORM, bool bIsLightSource = false);

	inline float GetWidth() { return m_Width; }
	inline float GetHeight() { return m_Height; }

private:
	bool InitializeASData(float width, float height, const WCHAR* pszNAME, bool bIsLightSource);

private:
	static UINT ms_QuadCount;
	static UINT ms_LightQuadCount;

	float m_Width = 0.0f;
	float m_Height = 0.0f;
};

class Box final : public Object
{
public:
	Box() = default;
	~Box() { --ms_BoxCount; }

	bool Initialize(Application* pApp, float width, float height, float depth, UINT materialID, const DirectX::XMFLOAT4X4 TRANSFORM, bool bIsLightSource = false);

private:
	static UINT ms_BoxCount;
};

class Sphere final : public Object
{
public:
	Sphere() = default;
	~Sphere() { --ms_SphereCount; --ms_LightSphereCount; }

	bool Initialize(Application* pApp, float radius, UINT materialID, const DirectX::XMFLOAT4X4 TRANSFORM, bool bIsLightSource = false);

	inline DirectX::XMFLOAT3 GetCenter() { return m_Center; }
	inline float GetRadius() { return m_Radius; }

private:
	bool InitializeASData(float radius, const WCHAR* pszNAME, bool bIsLightSource);

private:
	static UINT ms_SphereCount;
	static UINT ms_LightSphereCount;

	DirectX::XMFLOAT3 m_Center;
	float m_Radius = 0.0f;
};
