#pragma once

#include <SimpleMath.h>
#include "KnM.h"

using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Matrix;

class Renderer;

class Camera
{
public:
	//Camera() { UpdateViewDir(); }
	Camera() = default;
	~Camera() = default;

	void UpdateViewDir();
	void UpdateKeyboard(const float DELTA_TIME, Keyboard* pKeyboard);
	void UpdateMouse(const float MOUSE_NDC_X, const float MOUSE_NDC_Y);

	void MoveForward(const float DELTA_TIME);
	void MoveRight(const float DELTA_TIME);
	void MoveUp(const float DELTA_TIME);

	void Reset(const Vector3& POS, const float YAW, const float PITCH);

	Matrix GetView();
	Matrix GetProjection();
	inline Vector3 GetEyePos() { return m_Position; }
	inline Vector3 GetViewDir() { return m_ViewDirection; }
	inline Vector3 GetUpDir() { return m_UpDirection; }
	inline Vector3 GetRightDir() { return m_RightDirection; }
	inline float GetProjectionFovAngleY() { return m_ProjectionFovAngleY; }
	inline float GetAspectRatio() { return m_Aspect; }
	inline float GetNearZ() { return m_NearZ; }
	inline float GetFarZ() { return m_FarZ; }

	inline void SetAspectRatio(const float ASPECT_RATIO) { m_Aspect = ASPECT_RATIO; }
	inline void SetEyePos(const Vector3& POS) { m_Position = POS; }
	inline void SetViewDir(const Vector3& VIEW_DIR) { m_ViewDirection = VIEW_DIR; }
	inline void SetUpDir(const Vector3& UP_DIR) { m_UpDirection = UP_DIR; }
	inline void SetProjectionFovAngleY(const float ANGLE) { m_ProjectionFovAngleY = ANGLE; }
	inline void SetNearZ(const float NEAR_Z) { m_NearZ = NEAR_Z; }
	inline void SetFarZ(const float FAR_Z) { m_FarZ = FAR_Z; }

public:
	bool bUseFirstPersonView = true;

private:
	bool m_bUsePerspectiveProjection = true;

	Vector3 m_Position = Vector3(0.312183f, 0.957463f, -1.88458f);
	Vector3 m_ViewDirection = Vector3(0.0f, 0.0f, 1.0f);
	Vector3 m_UpDirection = Vector3(0.0f, 1.0f, 0.0f); // +Y �������� ����.
	Vector3 m_RightDirection = Vector3(1.0f, 0.0f, 0.0f);

	// roll, pitch, yaw
	float m_Yaw = -0.0589047f;
	float m_Pitch = 0.213803f;

	float m_Speed = 100.0f;

	float m_ProjectionFovAngleY = 45.0f;
	float m_NearZ = 0.01f;
	float m_FarZ = 10000.0f;
	float m_Aspect = 16.0f / 9.0f;
};

