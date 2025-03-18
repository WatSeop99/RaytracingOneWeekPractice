#pragma once

#include "CommonTypes.h"

bool CreateSphere(float radius, UINT32 sliceCount, UINT32 stackCount, std::vector<Vertex>* pOutVertices, std::vector<UINT32>* pOutIndices);
