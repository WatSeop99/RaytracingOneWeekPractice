#include "framework.h"
#include "IndexCreator.h"

IndexCreator::~IndexCreator()
{
	Check();
	Cleanup();
}

bool IndexCreator::Initialize(ULONG num)
{
	m_pIndexTable = new ULONG[num];
	if (!m_pIndexTable)
	{
		return false;
	}

	ZeroMemory(m_pIndexTable, sizeof(ULONG) * num);
	m_MaxNum = num;

	for (ULONG i = 0; i < m_MaxNum; ++i)
	{
		m_pIndexTable[i] = i;
	}

	return true;
}

bool IndexCreator::Cleanup()
{
	m_MaxNum = 0;
	m_AllocatedCount = 0;

	if (m_pIndexTable)
	{
		delete[] m_pIndexTable;
		m_pIndexTable = nullptr;
	}

	return true;
}

ULONG IndexCreator::Alloc()
{
	ULONG result = 0xFFFF;

	if (m_AllocatedCount >= m_MaxNum)
	{
		goto LB_RET;
	}

	result = m_pIndexTable[m_AllocatedCount];
	++m_AllocatedCount;

LB_RET:
	return result;
}

void IndexCreator::Free(ULONG index)
{
	if (!m_AllocatedCount)
	{
		__debugbreak();
	}
	--m_AllocatedCount;
	m_pIndexTable[m_AllocatedCount] = index;
}

void IndexCreator::Check()
{
	if (m_AllocatedCount)
	{
		__debugbreak();
	}
}
