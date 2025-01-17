#pragma once

#include <cstdint>

namespace GLAttributePosition
{
	enum : uint32_t
	{
		Position = 0,
		Normal = 1,
		Color = 2,
		UV = 3,
		ChunkData = 4,
	};
}

namespace GLUniformBindings
{
	enum : uint32_t
	{
		ProjectionInfoBlock = 1,
		TransformBlock = 2,
		ResolutionBlock = 4,
		FogParamBlock = 8,
		PointLight = 10,
		DirectionalLight = 11,
		SpotLight = 12
	};
}

namespace GLTextureBindings
{
	enum : uint32_t
	{
		GBuffer0 = 0,
		GBuffer1 = 1,
		Depth = 2,
		TextTexture = 3,
		BlockInfo = 4,
		SSAOSamples = 5,
		SSAONoise = 6,
		SSAOTexture = 7,
	};
}