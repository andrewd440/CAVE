#pragma once
#include "Atlas\Behavior.h"

class FCamera;

class CFlyingCamera : public Atlas::FBehavior
{
public:
	CFlyingCamera();
	~CFlyingCamera();

	void OnStart() override;
	void Update() override;

private:
	FCamera* mCamera;
	float    mMoveSpeed;
	float    mLookSpeed;
};

