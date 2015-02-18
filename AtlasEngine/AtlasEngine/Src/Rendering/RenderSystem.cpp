#include "Rendering\RenderSystem.h"
#include "ResourceHolder.h"
#include "Input\ButtonEvent.h"
#include "Rendering\ChunkManager.h"
#include "Rendering\Camera.h"
#include "Rendering\Screen.h"
#include "STime.h"
#include "Debugging\DebugDraw.h"
#include "Debugging\DebugText.h"
#include "Rendering\LightSystems.h"
#include <GL\glew.h>

namespace
{
	enum TransformBuffer : uint32_t
	{
		Model = 0,
		View = 64,
		Projection = 128,
		Size = 192
	};
}

FRenderSystem::FRenderSystem(Atlas::FWorld& World, sf::Window& GameWindow, FChunkManager& ChunkManager)
	: ISystem(World)
	, mWindow(GameWindow)
	, mChunkManager(ChunkManager)
	, mTransformBuffer(GLUniformBindings::TransformBlock, TransformBuffer::Size)
	, mDeferredRender()
	, mGBuffer(SScreen::GetResolution(), GL_RGBA32UI, GL_RGBA32F)
{
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glClearColor(0, 0, 0, 1.0f);

	LoadShaders();
	LoadSubSystems();

	// Initialize debug text
	FDebug::Draw* DebugDraw = new FDebug::Draw;
	FDebug::Text* DebugText = new FDebug::Text;
	DebugText->SetStyle("Vera.ttf", 20, FDebug::Text::AtlasInfo{ 512, 512, 1 });
	

	// Setup transform buffer with default values
	mTransformBuffer.SetData(TransformBuffer::View, FCamera::Main->Transform.WorldToLocalMatrix());
	mTransformBuffer.SetData(TransformBuffer::Projection, FCamera::Main->GetProjection());
}

void FRenderSystem::LoadShaders()
{
	// Load all main rendering shaders
	SShaderHolder::Load("DeferredRender.vert", L"Shaders/DeferredRender.vert", GL_VERTEX_SHADER);
	SShaderHolder::Load("DeferredRender.frag", L"Shaders/DeferredRender.frag", GL_FRAGMENT_SHADER);
	SShaderHolder::Load("FullScreenQuad.vert", L"Shaders/FullScreenQuad.vert", GL_VERTEX_SHADER);
	SShaderHolder::Load("DeferredPointLighting.frag", L"Shaders/DeferredPointLighting.frag", GL_FRAGMENT_SHADER);
	SShaderHolder::Load("DeferredLightingCommon.frag", L"Shaders/DeferredLightingCommon.frag", GL_FRAGMENT_SHADER);
	SShaderHolder::Load("DeferredSpotLighting.frag", L"Shaders/DeferredSpotLighting.frag", GL_FRAGMENT_SHADER);
	SShaderHolder::Load("DeferredDirectionalLighting.frag", L"Shaders/DeferredDirectionalLighting.frag", GL_FRAGMENT_SHADER);
	
	
	// Load render shaders
	mDeferredRender.AttachShader(SShaderHolder::Get("DeferredRender.vert"));
	mDeferredRender.AttachShader(SShaderHolder::Get("DeferredRender.frag"));
	mDeferredRender.LinkProgram();
}

void FRenderSystem::LoadSubSystems()
{
	AddSubSystem<FDirectionalLightSystem>();
	AddSubSystem<FPointLightSystem>();
	AddSubSystem<FSpotLightSystem>();
}

FRenderSystem::~FRenderSystem()
{
	delete FDebug::Draw::GetInstancePtr();
	delete FDebug::Text::GetInstancePtr();
}

void FRenderSystem::SetModelTransform(const FTransform& WorldTransform)
{
	mTransformBuffer.SetData(TransformBuffer::Model, WorldTransform.LocalToWorldMatrix());
}

void FRenderSystem::Update()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GeometryPass();
	LightingPass();


	// Render debug draws
	//FDebug::Draw::GetInstance().Render();

	// Debug Print
	auto& DebugText = FDebug::Text::GetInstance();
	const Vector3f CameraPosition = FCamera::Main->Transform.GetPosition();
	wchar_t String[250];
	const Vector3f Direction = FCamera::Main->Transform.GetRotation() * -Vector3f::Forward;
	swprintf_s(String, L"FPS: %.2f   Position: %.1f %.1f %.1f Direction: %.1f %.1f %.1f", 1.0f / STime::GetDeltaTime(), CameraPosition.x, CameraPosition.y, CameraPosition.z, Direction.x, Direction.y, Direction.z);
	DebugText.AddText(String, FColor(1.0f, .8f, 1.0f, .6f), Vector2i(50, SScreen::GetResolution().y - 50));

	swprintf_s(String, L"Chunks used: %d", FChunk::ChunkAllocator.Size());
	DebugText.AddText(String, FColor(1.0f, .8f, 1.0f, .6f), Vector2i(50, SScreen::GetResolution().y - 100));

	Vector3i ChunkPosition = Vector3i(CameraPosition.x / FChunk::CHUNK_SIZE, CameraPosition.y / FChunk::CHUNK_SIZE, CameraPosition.z / FChunk::CHUNK_SIZE);
	swprintf_s(String, L"Chunk Position: %d %d %d", ChunkPosition.x, ChunkPosition.y, ChunkPosition.z);
	DebugText.AddText(String, FColor(1.0f, .8f, 1.0f, .6f), Vector2i(50, SScreen::GetResolution().y - 150));

	DebugText.Render();

	// Display renderings
	mWindow.display();
}

void FRenderSystem::GeometryPass()
{
	// Send view transform to gpu
	mTransformBuffer.SetData(TransformBuffer::View, FCamera::Main->Transform.WorldToLocalMatrix());

	// Open G-Buffer for writing and enable deferred render shader.
	mGBuffer.StartWrite();
	mDeferredRender.Use();

	// Make sure depth testing is enabled
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	// Render geometry
	mChunkManager.Render(*this);
	
	// Close the G-Buffer
	mGBuffer.EndWrite();
}

void FRenderSystem::LightingPass()
{
	auto& SubSystems = GetSubSystems();

	// No depth testing for lights and set light blending settings.
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);

	 // Enable G-Buffer for reading
	mGBuffer.StartRead();

	for (auto& SubSystem : SubSystems)
		SubSystem->Update();

	mGBuffer.EndRead();
	glDisable(GL_BLEND);
}