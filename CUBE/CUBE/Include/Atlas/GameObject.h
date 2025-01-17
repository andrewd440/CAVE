#pragma once
#include "Bitsize.h"
#include <vector>
#include <memory>
#include "ComponentTypes.h"
#include "Math\Transform.h"
#include "BulletPhysics\btBulletCollisionCommon.h"
#include <map>
#include <typeindex>

class FChunkManager;

namespace Atlas
{
	class IComponent;
	class FBehavior;

	/**
	* Used to represent any game object. 
	* Acts as a grouping/container for Components.
	*/
	class FGameObject : public btMotionState
	{
	public:
		using ID = uint32_t;
		static const uint32_t NULL_COMPONENT = 0xFFFFFFFF; // Use max number for null index

	public:
		FTransform Transform;

	public:
		~FGameObject() {}
		FGameObject(const FGameObject& Other) = delete;
		FGameObject& operator=(const FGameObject& Other) = delete;

		template <EComponent::Type Type>
		/**
		* Retrieves a specified component from the GameObject.
		* @return IComponent casted to it's derived type
		*/
		typename ComponentTraits::Object<Type>::Type& GetComponent();

		template <EComponent::Type Type>
		/**
		* Adds a component to the GameObject.
		* @return A reference to the added component.
		*/
		typename ComponentTraits::Object<Type>::Type& AddComponent();

		/**
		* Removes a specified IComponent from the FGameObject.
		*/
		void RemoveComponent(const EComponent::Type Type);

		template <typename Type>
		/**
		* Retrieve a behavior component.
		*/
		Type* GetBehavior();
		
		template <typename Type>
		/**
		* Add a behavior component.
		*/
		Type* AddBehavior();

		template <typename Type>
		/**
		* Remove a behavior component.
		*/
		void RemoveBehavior();

		FChunkManager& GetChunkManager();

		/**
		* Retrieves all Components that are attached to the GameObject.
		* @return All Components owned by the FGameObject
		*/
		std::vector<IComponent*> GetAllComponents();

		/**
		* Removes all component current attached to this GameObject.
		*/
		void RemoveAllComponents();

		/**
		* Retrieves the ID assigned to the FGameObject
		* @return The ID
		*/
		ID GetID() const;

		/**
		* Checks if the FGameObject is active
		* @return True if FGameObject is active
		*/
		bool IsActive() const;

		/**
		* Sets the active state of the FGameObject. Inactive entity components are not
		* processed.
		* @param Flag - Status of the GameObject
		*/
		void SetActive(bool Flag);

		/**
		* Sets this GameObject to be destroyed. Objects are not instantly destroyed,
		* but destroy on the next GO manager update.
		*/
		void Destroy();

		/**
		* Bullet Physics callback for retrieving the current motion
		* state.
		*/
		void getWorldTransform(btTransform& WorldTransform) const override;

		/**
		* Bullet Physics callback for setting the current motion state
		* of this object based on rigidbody physics.
		*/
		void setWorldTransform(const btTransform& WorldTransform) override;

		FGameObject& CreateGameObject();

	private:
		friend class FGameObjectManager;
		friend class FSystemManager;
		friend class ISystem;

		FGameObject(FGameObjectManager& GOManager, FChunkManager& ChunkManager);	// Only the GameObject Manager creates Entities

		/**
		* Calls OnStart on all behavior components.
		* Should be called at the start of the game.
		*/
		void OnStart();

		/**
		* Updates all behavior components for
		* this object.
		*/
		void Update();

		/**
		* Sets the ID for the GameObject.
		* This is not a GUID, it is just an index in the GameObject Managers' container.
		* @param ID - ID to be assigned
		*/
		void SetID(const ID ID);

		/**
		* Removes a system bit from the GameObject
		* @param Bit - bit to be removed
		*/
		void RemoveSystemBit(const std::bitset<BITSIZE>& Bit);

		/**
		* Retrieves the system bits assigned to this GameObject
		* @return Set of system bits
		*/
		std::bitset<BITSIZE> GetSystemBitMask() const;


		/**
		* Retrieves the component bits in use by this GameObject.
		*/
		std::bitset<BITSIZE> GetComponentBitMask() const;

		/**
		* Adds a component bit to the GameObject.
		* Used by the GameObject Manager when a component is added to a GameObject.
		* @param Bit - The bit to be added
		*/
		void AddComponentBit(const std::bitset<BITSIZE>& Bit);

		/**
		* Removes a component bit from the GameObject.
		* Used by the GameObject Manager when a component is removed from a GameObject.
		* @param Bit - Bit to be removed
		*/
		void RemoveComponentBit(const std::bitset<BITSIZE>& Bit);

		/**
		* Sets a System bit to be assigned to the GameObject
		* @param Bit - System bit to be added
		*/
		void SetSystemBit(const std::bitset<BITSIZE>& Bit);

	private:
		std::bitset<BITSIZE>              mComponentBits; // Property component types currently attached.
		std::bitset<BITSIZE>              mSystemBits;    // Systems currently registered in.
		FGameObjectManager&               mGOManager;
		FChunkManager&                    mChunkManager;

		uint32_t						  mComponents[EComponent::Count]; // Handles for common property components.
		std::map<std::type_index, FBehavior*>   mBehaviors;
		ID		                          mID;            // Non-unique id for this GO.
		bool                              mIsActive;      // If not active, this GO's components will not be processed.
	};
}

#include "GameObject.inl"