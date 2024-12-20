#pragma once

typedef BOOL(WINAPI* LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

class BitmapBuffer;
class camera;
class hittable_list;

enum THREAD_EVENT
{
	THREAD_EVENT_COMPUTE = 0,
	THREAD_EVENT_DESTROY,
	THREAD_EVENT_TOTAL_COUNT
};

struct ThreadParam
{
	int ThreadNo;
	int TotalThreadCount;
	BitmapBuffer* pBitmapBuffer;
	camera* pCamera;
	hittable_list* pWorld;
	HANDLE hEventList[THREAD_EVENT_TOTAL_COUNT];
	HANDLE* phCompleted;
};

DWORD CountSetBits(ULONG_PTR bitMask);
BOOL GetPhysicalCoreCount(DWORD* pdwOutPhysicalCoreCount, DWORD* pdwOutLogicalCoreCount);

UINT WINAPI ThreadFunc(void* pArg);
