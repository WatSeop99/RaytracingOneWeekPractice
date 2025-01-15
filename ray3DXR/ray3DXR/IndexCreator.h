#pragma once

class IndexCreator
{
public:
	IndexCreator() = default;
	~IndexCreator();

	bool Initialize(ULONG num);
	bool Cleanup();

	ULONG Alloc();
	void Free(ULONG index);

	void Check();

private:
	ULONG* m_pIndexTable = nullptr;
	ULONG m_MaxNum = 0;
	ULONG m_AllocatedCount = 0;
};
