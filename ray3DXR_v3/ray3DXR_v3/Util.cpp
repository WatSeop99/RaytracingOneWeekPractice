#include "pch.h"
#include "Util.h"

double RandomDouble(double from, double to)
{
	double range = to - from;
	double div = RAND_MAX / range;
	return from + (rand() / div);
}

float RandomFloat(float from, float to)
{
	float range = to - from;
	float div = RAND_MAX / range;
	return from + (rand() / div);
}

double RandomDouble()
{
	return RandomDouble(0, 1.0);
}

float RandomFloat()
{
	return RandomFloat(0.0f, 1.0f);
}

void RandomColor(DirectX::XMVECTOR* pOutColor)
{
	_ASSERT(pOutColor);
	*pOutColor =
	{
		RandomFloat(),
		RandomFloat(),
		RandomFloat()
	};
}
