#pragma once

#include "Object.h"

class Box : public Object
{
public:
	Box() = default;
	~Box() = default;

	bool Initialize();
	bool Cleanup();

private:

};

