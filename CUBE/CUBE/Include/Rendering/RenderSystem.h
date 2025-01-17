#pragma once
#include "Atlas\System.h"
#include "SFML\Window\Window.hpp"
#include "Rendering\UniformBlockStandard.h"
#include "Rendering\ShaderProgram.h"
#include "Rendering\GBuffer.h"
#include "Math\Box.h"
#include "ImageEffects\IImageEffect.h"
#include "Utils\Event.h"
#include "Math\Vector2.h"

class FChunkManager;

class FRenderSystem : public Atlas::ISystem
{
public:
	static TEvent<Vector2ui> OnResolutionChange;

public:
	/**
	* Constructs a rendering system.
 	* @param World - The world this system is part of.
	* @param GameWindow - The window to render to.
	* @param ChunkManager - The manager of chunks
	*/
	FRenderSystem(Atlas::FWorld& World, sf::Window& GameWindow, FChunkManager& ChunkManager);
	~FRenderSystem();

	/**
	* Sets the current model transform for render calls. This is combined
	* with the current camera transform to produce the modelview transform.
	*/
	void SetModelTransform(const FTransform& WorldTransform);

	/**
	* Renders the scene.
	*/
	void Update() override;

	/**
	* Sets the resolution of the rendering display.
	*/
	void SetResolution(const Vector2ui& Resolution);

	/**
	* Sends draw calls to all the currently visible geometry with respect to
 	* the main camera.
	*/
	void RenderGeometry();

	/**
	* Get the current world space coordinates of each of the 8 corners
	* of the view volume.
	*/
	//FBox GetViewBounds() const;

	/**
	* Adds a rendering post process technique.
	* @return The id of the postprocess.
	*/
	uint32_t AddPostProcess(std::unique_ptr<IImageEffect> PostProcess);

	/**
	* Enables a rendering post process that was previously added.
	*/
	void EnablePostProcess(const uint32_t ID);

	/**
	* Disables a rendering post process.
	*/
	void DisablePostProcess(const uint32_t ID);

private:
	void AllocateGBuffer(const Vector2ui& Resolution);

	/**
	* Load all rendering based shaders to the
	* shader manager.
	*/
	void LoadShaders();

	/**
	* Loads all subsystems used for rendering.
	*/
	void LoadSubSystems();

	/**
	* Construct a GBuffer from the main camera.
	*/
	void ConstructGBuffer();

	/**
	* Process all of the scene lighting.
	*/
	void LightingPass();

	/**
	* Updates the bounding box for the current view volume.
	*/
	//void UpdateViewBounds();

	/**
	* Transfers view and projection data to shaders.
	*/
	void TransferViewProjectionData();

	void Start() override;

private:
	// Each type of subsystem used by the rendering system
	struct SubSystems
	{
		enum Type : uint32_t
		{
			DirectionalLight,
			PointLight,
			Count
		};
	};

private:
	struct PostProcessRecord
	{
		PostProcessRecord(std::unique_ptr<IImageEffect> PostProcess)
			: Process(std::move(PostProcess))
			, IsActive(false)
		{}
		PostProcessRecord(PostProcessRecord& Other)
			: Process(std::move(Other.Process))
			, IsActive(Other.IsActive)
		{}
		std::unique_ptr<IImageEffect> Process;
		bool IsActive;
	};
	using PostProcessContainer = std::vector<PostProcessRecord>;

private:
	sf::Window&           mWindow;
	FChunkManager&        mChunkManager;
	FShaderProgram        mDeferredRender;
	FShaderProgram        mChunkRender;
	PostProcessContainer  mPostProcesses;
	//FBox                  mViewAABB;

	struct GBuffer
	{
		GLuint FBO;
		GLuint DepthTex;
		GLuint ColorTex[1];
	} mGBuffer;

	// Shader info blocks and buffers
	FUniformBlock   mTransformBlock;
	FUniformBlock   mResolutionBlock;
	FUniformBlock   mProjectionInfoBlock;
	GLuint          mBlockInfoBuffer;
};