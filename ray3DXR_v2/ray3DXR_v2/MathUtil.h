#pragma once

#define ALIGN(v, powerOf2Alignment) (((v) + (powerOf2Alignment)-1) & ~((powerOf2Alignment)-1))

float RandomFloat();
float RandomFloat(float from, float to);
double RandomDouble();
double RandomDouble(double from, double to);
DirectX::XMVECTOR __vectorcall RandomColor();
float Clamp(float val, float lower, float upper);
