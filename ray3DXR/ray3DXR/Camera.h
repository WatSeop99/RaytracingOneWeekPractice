#pragma once

class Camera
{
public:
	Camera() = default;
	~Camera() = default;

	DirectX::XMFLOAT4X4 GetView();
	DirectX::XMFLOAT4X4 GetProjection();
	inline DirectX::XMFLOAT3 GetEyePos() { return m_Position; }
	inline DirectX::XMFLOAT3 GetViewDir() { return m_ViewDirection; }
	inline DirectX::XMFLOAT3 GetUpDir() { return m_UpDirection; }
	inline DirectX::XMFLOAT3 GetRightDir() { return m_RightDirection; }
	inline float GetProjectionFovAngleY() { return m_ProjectionFovAngleY; }
	inline float GetAspectRatio() { return m_Aspect; }
	inline float GetNearZ() { return m_NearZ; }
	inline float GetFarZ() { return m_FarZ; }

	inline void SetAspectRatio(const float ASPECT_RATIO) { m_Aspect = ASPECT_RATIO; }
	inline void SetEyePos(const DirectX::XMFLOAT3 POS) { m_Position = POS; }
	inline void SetViewDir(const DirectX::XMFLOAT3 VIEW_DIR) { m_ViewDirection = VIEW_DIR; }
	inline void SetUpDir(const DirectX::XMFLOAT3 UP_DIR) { m_UpDirection = UP_DIR; }
	inline void SetProjectionFovAngleY(const float ANGLE) { m_ProjectionFovAngleY = ANGLE; }
	inline void SetNearZ(const float NEAR_Z) { m_NearZ = NEAR_Z; }
	inline void SetFarZ(const float FAR_Z) { m_FarZ = FAR_Z; }

private:
	DirectX::XMFLOAT3 m_Position;
	DirectX::XMFLOAT3 m_ViewDirection;
	DirectX::XMFLOAT3 m_UpDirection = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
	DirectX::XMFLOAT3 m_RightDirection = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);

	float m_Yaw = -0.0589047f;
	float m_Pitch = 0.213803f;

	float m_Speed = 3.0f;

	float m_ProjectionFovAngleY = 45.0f;
	float m_NearZ = 0.01f;
	float m_FarZ = 100.0f;
	float m_Aspect = 16.0f / 9.0f;
};
