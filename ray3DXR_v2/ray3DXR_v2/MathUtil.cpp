#include <random>
#include <DirectXMath.h>
#include "MathUtil.h"

float RandomFloat()
{
	return RandomFloat(0.0f, 1.0f);
}

float RandomFloat(float from, float to)
{
	static std::uniform_real_distribution<float> s_Distribution(from, to);
	static std::mt19937 s_Generator;
	return s_Distribution(s_Generator);
}

double RandomDouble()
{
	return RandomDouble(0.0, 1.0);
}

double RandomDouble(double from, double to)
{
	static std::uniform_real_distribution<double> s_Distribution(from, to);
	static std::mt19937 s_Generator;
	return s_Distribution(s_Generator);
}

DirectX::XMVECTOR __vectorcall RandomColor()
{
	return { RandomFloat(), RandomFloat(), RandomFloat() };
}

float Min(float a, float b)
{
	return (a < b ? a : b);
}
float Max(float a, float b)
{
	return (a > b ? a : b);
}
float Clamp(float val, float lower, float upper)
{
	return Min(Max(val, lower), upper);
}
