#pragma once

#include "Atlas\World.h"
#include "SFML\Window\Window.hpp"
#include "Rendering\ChunkManager.h"

/**
* Root/Central class for the game engine
*/
class FVoxiGineRoot
{
public:
	FVoxiGineRoot();

	~FVoxiGineRoot();

	// Disable copying of this object.
	FVoxiGineRoot(const FVoxiGineRoot& Other) = delete;
	FVoxiGineRoot& operator=(const FVoxiGineRoot& Other) = delete;

	void Start();

	void GameLoop();

private:
	void ServiceEvents();
	void UpdateTimers();

private:
	struct Systems
	{
		enum Index : uint32_t
		{
			Render,
			Count
		};
	};

private:
	sf::Window mGameWindow;
	Atlas::FWorld mWorld;
	FChunkManager mChunkManager;
};
