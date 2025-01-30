#include "framework.h"
#include "Camera.h"

DirectX::XMFLOAT4X4 Camera::GetView()
{
	DirectX::XMFLOAT4X4 ret;
	DirectX::XMMATRIX temp = DirectX::XMMatrixMultiply(DirectX::XMMatrixTranslation(-m_Position.x, -m_Position.y, -m_Position.z), DirectX::XMMatrixRotationY(-m_Yaw));
	temp = DirectX::XMMatrixMultiply(temp, DirectX::XMMatrixRotationX(-m_Pitch));
	DirectX::XMStoreFloat4x4(&ret, temp);

	return ret;
}

DirectX::XMFLOAT4X4 Camera::GetProjection()
{
	DirectX::XMMATRIX temp = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(m_ProjectionFovAngleY), m_Aspect, m_NearZ, m_FarZ);
	DirectX::XMFLOAT4X4 ret;
	DirectX::XMStoreFloat4x4(&ret, temp);

	return ret;
}
