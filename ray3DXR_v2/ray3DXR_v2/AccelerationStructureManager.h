#pragma once

class AccelerationStructureManager final
{
public:
	AccelerationStructureManager() = default;
	~AccelerationStructureManager() = default;

	void AddBottomlevelAS();
	UINT AddBottomLevelASInstance();
	bool InitializeTopLevelAS();
	bool Build();


private:

};
