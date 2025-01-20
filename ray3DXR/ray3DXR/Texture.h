#pragma once

interface Texture
{
	virtual DirectX::XMFLOAT3 Value() = 0;
};

class SolidColor final : public Texture
{
public:
	SolidColor(const DirectX::XMFLOAT3& COLOR) : m_Color(COLOR) { }
	~SolidColor() = default;

	inline DirectX::XMFLOAT3 Value() override { return m_Color; }

private:
	DirectX::XMFLOAT3 m_Color;
};
