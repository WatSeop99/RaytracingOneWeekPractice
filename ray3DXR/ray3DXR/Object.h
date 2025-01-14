#pragma once

class Object
{
public:
	Object() = default;
	virtual ~Object() { Cleanup(); }

	bool Cleanup();

protected:
	ID3D12Resource* m_pVertexBuffer = nullptr;
	ID3D12Resource* m_pIndexBuffer = nullptr;
};
