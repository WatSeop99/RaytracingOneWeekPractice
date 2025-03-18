#include "framework.h"
#include "GeometryUtil.h"

bool CreateSphere(float radius, UINT32 sliceCount, UINT32 stackCount, std::vector<Vertex>* pOutVertices, std::vector<UINT32>* pOutIndices)
{
	_ASSERT(radius > 0.0f);
	_ASSERT(sliceCount > 0);
	_ASSERT(stackCount > 0);
	_ASSERT(pOutVertices);
	_ASSERT(pOutIndices);

	Vertex topVertex = { { 0.0f, +radius, 0.0f }, 0.0, { 0.0f, 1.0f, 0.0f }, 0.0 };

	pOutVertices->push_back(topVertex);

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f * XM_PI / sliceCount;

	// Compute vertices for each stack ring (do not count the poles as rings).
	for (UINT32 i = 1; i <= stackCount - 1; ++i)
	{
		float phi = i * phiStep;

		// Vertices of ring.
		for (UINT32 j = 0; j <= sliceCount; ++j)
		{
			float theta = j * thetaStep;

			Vertex v;
			// spherical to cartesian
			v.Position.x = radius * sinf(phi) * cosf(theta);
			v.Position.y = radius * cosf(phi);
			v.Position.z = radius * sinf(phi) * sinf(theta);

			XMVECTOR p = XMLoadFloat3(&v.Position);
			XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

			pOutVertices->push_back(v);
		}
	}

	Vertex bottomVertex = { { 0.0f, -radius, 0.0f }, 0.0, { 0.0f, -1.0f, 0.0f }, 0.0 };
	pOutVertices->push_back(bottomVertex);


	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//
	for (UINT32 i = 1; i <= sliceCount; ++i)
	{
		pOutIndices->push_back(0);
		pOutIndices->push_back(i + 1);
		pOutIndices->push_back(i);
	}

	//
	// Compute indices for inner stacks (not connected to poles).
	//
	UINT32 baseIndex = 1;
	UINT32 ringVertexCount = sliceCount + 1;
	for (UINT32 i = 0; i < stackCount - 2; ++i)
	{
		for (UINT32 j = 0; j < sliceCount; ++j)
		{
			pOutIndices->push_back(baseIndex + i * ringVertexCount + j);
			pOutIndices->push_back(baseIndex + i * ringVertexCount + j + 1);
			pOutIndices->push_back(baseIndex + (i + 1) * ringVertexCount + j);

			pOutIndices->push_back(baseIndex + (i + 1) * ringVertexCount + j);
			pOutIndices->push_back(baseIndex + i * ringVertexCount + j + 1);
			pOutIndices->push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	UINT32 southPoleIndex = (UINT32)pOutVertices->size() - 1;

	// Offset the indices to the index of the first vertex in the last ring.
	baseIndex = southPoleIndex - ringVertexCount;

	for (UINT32 i = 0; i < sliceCount; ++i)
	{
		pOutIndices->push_back(southPoleIndex);
		pOutIndices->push_back(baseIndex + i);
		pOutIndices->push_back(baseIndex + i + 1);
	}


	return true;
}
