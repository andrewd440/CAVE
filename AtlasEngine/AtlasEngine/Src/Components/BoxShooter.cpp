#include "Components\BoxShooter.h"
#include "Input\ButtonEvent.h"
#include "Rendering\Camera.h"
#include "Atlas\GameObject.h"
#include "Components\MeshComponent.h"

CBoxShooter::CBoxShooter()
{
}


CBoxShooter::~CBoxShooter()
{
}

void CBoxShooter::Update()
{
	if (SButtonEvent::GetKeyDown(sf::Keyboard::Space))
	{
		FCamera& Camera = *FCamera::Main;

		auto& Box = CreateGameObject();
		Box.Transform.SetPosition(Camera.Transform.GetPosition() + Vector3f::Up * 2.0f);
		Box.Transform.SetScale(Vector3f{ .5f, .5f, .5f });

		auto& Body = Box.AddComponent<Atlas::EComponent::RigidBody>();
		Vector3f Forward = Camera.Transform.GetRotation() * -Vector3f::Forward;
		Body.Body.setLinearVelocity(btVector3{ Forward.x, Forward.y, Forward.z } *40.0f);
		Body.BoxCollider.setImplicitShapeDimensions(btVector3{ .5f, .5f, .5f });

		auto& Mesh = Box.AddComponent<Atlas::EComponent::Mesh>();
		Mesh.LoadModel("Box.obj");
	}
}