#include "..\..\Include\Rendering\Chunk.h"
#include "..\..\Include\Rendering\UniformBlockStandard.h"
#include "..\..\Include\Debugging\ConsoleOutput.h"
#include "Debugging\DebugText.h"
#include "Rendering\Screen.h"
#include "Rendering\ChunkManager.h"
#include "Physics\PhysicsSystem.h"

FPoolAllocator<sizeof(FBlock) * FChunk::BLOCKS_PER_CHUNK, FChunk::POOL_SIZE> FChunk::ChunkAllocator(__alignof(FChunk));
FPoolAllocatorType<TMesh<FVoxelVertex>, FChunk::POOL_SIZE * 2> FChunk::MeshAllocator(__alignof(TMesh<FVoxelVertex>));
FPoolAllocatorType<FChunk::CollisionData, FChunk::POOL_SIZE> FChunk::CollisionAllocator(__alignof(FChunk::CollisionData));

int32_t FChunk::BlockIndex(Vector3i Position)
{
	ASSERT(Position.x >= 0 && Position.x < CHUNK_SIZE &&
		Position.y >= 0 && Position.y < CHUNK_SIZE &&
		Position.z >= 0 && Position.z < CHUNK_SIZE);

	const Vector3i PositionToIndex{ CHUNK_SIZE, CHUNK_SIZE * CHUNK_SIZE, 1 };
	return Vector3i::Dot(Position, PositionToIndex);
}

int32_t FChunk::BlockIndex(int32_t X, int32_t Y, int32_t Z)
{
	return FChunk::BlockIndex(Vector3i{ X, Y, Z });
}

FChunk::FChunk()
	: mBlocks(nullptr)
	, mMesh()
	, mCollisionData(nullptr)
	, mIsLoaded(false)
	, mIsEmpty()
	, mActiveMesh()
{
	mActiveMesh.store(false);
	mIsEmpty[0] = true;
	mIsEmpty[1] = true;

	// Allocate mesh, block, and collision data
	mMesh[0] = new (MeshAllocator.Allocate()) TMesh < FVoxelVertex > {GL_STATIC_DRAW};
	mMesh[1] = new (MeshAllocator.Allocate()) TMesh < FVoxelVertex > {GL_STATIC_DRAW};
	mBlocks = static_cast<FBlock*>(ChunkAllocator.Allocate());

	// Construct Collision fields with new memory
	mCollisionData = new (CollisionAllocator.Allocate()) CollisionData{};

	// Set collision data
	CollisionData& CollisionInfo = *mCollisionData;

	// Set the aabb to the bounds of the chunk
	const btVector3 Min{ 0, 0, 0 };
	const btVector3 Max{ (float)FChunk::CHUNK_SIZE, (float)FChunk::CHUNK_SIZE, (float)FChunk::CHUNK_SIZE };
	CollisionInfo.Mesh.setPremadeAabb(Min, Max);

	// Set collision shape and transform
	CollisionInfo.Object.setCollisionShape(&CollisionInfo.Shape);
}

FChunk::~FChunk()
{
	ChunkAllocator.Free(mBlocks);
	MeshAllocator.Free(mMesh[0]);
	MeshAllocator.Free(mMesh[1]);
	CollisionAllocator.Free(mCollisionData);
}


void FChunk::Load(const std::vector<uint8_t>& BlockData, const Vector3f& WorldPosition)
{
	ASSERT(!mIsLoaded);

	// Set collision data
	CollisionData& CollisionInfo = *mCollisionData;

	// Set collision transform
	CollisionInfo.Object.setWorldTransform(btTransform{ btQuaternion{0,0,0},
				btVector3{ WorldPosition.x, WorldPosition.y, WorldPosition.z } });

	// Current index to access block type
	int32_t TypeIndex = 0;
	int32_t DataSize = BlockData.size();
	
	// Write RLE data for chunk
	for (int32_t y = 0; y < CHUNK_SIZE && TypeIndex < DataSize; y++)
	{
		for (int32_t x = 0; x < CHUNK_SIZE; x++)
		{
			int32_t BaseIndex = BlockIndex(x, y, 0);
			for (int32_t z = 0; z < CHUNK_SIZE;)
			{
				uint8_t Count = 0;
				uint8_t RunLength = BlockData[TypeIndex + 1];
				for (Count = 0; Count < RunLength; Count++)
				{
					mBlocks[BaseIndex + z + Count].Type = (FBlock::BlockType)BlockData[TypeIndex];
				}

				z += RunLength;
				TypeIndex += 2;
			}
		}
	}


	mIsLoaded = true;
}

void FChunk::Unload(std::vector<uint8_t>& BlockDataOut)
{
	ASSERT(mIsLoaded);

	mIsLoaded = false;

	// Extract RLE data for chunk
	for (int32_t y = 0; y < CHUNK_SIZE; y++)
	{
		for (int32_t x = 0; x < CHUNK_SIZE; x++)
		{
			for (int32_t z = 0; z < CHUNK_SIZE;)
			{
				const uint32_t CurrentBlockIndex = BlockIndex(x, y, z);
				const FBlock::BlockType CurrentBlock = mBlocks[CurrentBlockIndex].Type;
				uint8_t Length = 1;

				FBlock::BlockType NextBlock = mBlocks[CurrentBlockIndex + (int32_t)Length].Type;
				while (CurrentBlock == NextBlock && Length + z < CHUNK_SIZE)
				{
					Length++;
					NextBlock = mBlocks[CurrentBlockIndex + (int32_t)Length].Type;
				}

				// Append RLE data
				BlockDataOut.insert(BlockDataOut.end(), { CurrentBlock, Length });
				z += (int32_t)Length;
			}
		}
	}
}

void FChunk::ShutDown(FPhysicsSystem& PhysicsSystem)
{
	// Remove collision data from physics system
	if (!mIsEmpty[mActiveMesh])
		PhysicsSystem.RemoveCollider(mCollisionData->Object);

	mMesh[0]->ClearData();
	mMesh[0]->Deactivate();
	mMesh[1]->ClearData();
	mMesh[1]->Deactivate();
	mActiveMesh = false;
}

bool FChunk::IsLoaded() const
{
	return mIsLoaded;
}

void FChunk::Render(const GLenum RenderMode)
{
	ASSERT(mIsLoaded);

	mMesh[mActiveMesh]->Render(RenderMode);
}

void FChunk::SwapMeshBuffer(FPhysicsSystem& PhysicsSystem)
{
	mMesh[mActiveMesh]->ClearData();
	mMesh[mActiveMesh]->Deactivate();
	mActiveMesh = !mActiveMesh;
	mMesh[mActiveMesh]->Activate();

	auto& Mesh = mMesh[mActiveMesh];

	// Update collision info
	if (!mIsEmpty[mActiveMesh] && Mesh->GetVertexCount() > 0)
	{
		// Set vertex properties for collision mesh
		const int32_t IndexStride = 3 * sizeof(uint32_t);
		const int32_t VertexStride = sizeof(FVoxelVertex);

		// Build final collision mesh
		btIndexedMesh VertexData;
		VertexData.m_triangleIndexStride = IndexStride;
		VertexData.m_numTriangles = (int)Mesh->GetIndexCount() / 3;;
		VertexData.m_numVertices = (int)Mesh->GetVertexCount();
		VertexData.m_triangleIndexBase = (const unsigned char*)Mesh->GetIndices();
		VertexData.m_vertexBase = (const unsigned char*)Mesh->GetVertices();
		VertexData.m_vertexStride = VertexStride;

		// Reconstruct the collision shape with updated data
		mCollisionData->Mesh.getIndexedMeshArray().clear();
		mCollisionData->Mesh.getIndexedMeshArray().push_back(VertexData);
		mCollisionData->Shape.~btBvhTriangleMeshShape();
		new (&mCollisionData->Shape) btBvhTriangleMeshShape{ &mCollisionData->Mesh, false };

		if (mIsEmpty[!mActiveMesh])
		{
			// Previous mesh was empty
			PhysicsSystem.AddCollider(mCollisionData->Object);
		}
	}
	else if (!mIsEmpty[!mActiveMesh])
	{
		// Previous mesh was not empty but this one is
		PhysicsSystem.RemoveCollider(mCollisionData->Object);
	}
}

void FChunk::RebuildMesh()
{
	// Mesh the chunk
	mIsEmpty[!mActiveMesh] = true;
	GreedyMesh();
}

void FChunk::SetBlock(const Vector3i& Position, FBlock::BlockType BlockType)
{
	mBlocks[BlockIndex(Position)].Type = BlockType;
}

FBlock::BlockType FChunk::GetBlock(const Vector3i& Position) const
{
	return mBlocks[BlockIndex(Position)].Type;
}

void FChunk::DestroyBlock(const Vector3i& Position)
{
	mBlocks[BlockIndex(Position)].Type = FBlock::None;
}

void FChunk::GreedyMesh()
{
	// Greedy mesh algorithm by Mikola Lysenko from http://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
	// Java implementation from https://github.com/roboleary/GreedyMesh/blob/master/src/mygame/Main.java

	// Cache position of this chunk to check neiboring chunks in ambient occlusion tests
	//const Vector3ui ChunkPosition = mChunkManager->GetChunkPosition(this);

	// Vertex and index data to be sent to the mesh
	std::vector<FVoxelVertex> Vertices;
	std::vector<uint32_t> Indices;

	// Variables to be used by the algorithm
	int32_t i, j, k, Length, Width, Height, u, v, n, Side = 0;
	int32_t x[3], q[3], du[3], dv[3];

	// This mask will contain matching voxel faces as we
	// loop through each direction in the chunk
	MaskInfo Mask[CHUNK_SIZE * CHUNK_SIZE];

	// Start with a for loop the will flip face direction once we iterate through
	// the chunk in one direction.
	for (bool BackFace = true, b = false; b != BackFace; BackFace = BackFace && b, b = !b)
	{

		// Iterate through each dimension of the chunk
		for (int32_t d = 0; d < 3; d++)
		{
			// Get the other 2 axes
			u = (d + 1) % 3;
			v = (d + 2) % 3;

			x[0] = 0;
			x[1] = 0;
			x[2] = 0;

			// Used to check against covering voxels
			q[0] = 0;
			q[1] = 0;
			q[2] = 0;
			q[d] = 1;

			if (d == 0)
			{ 
				Side = BackFace ? WEST : EAST; 
			}
			else if (d == 1) 
			{ 
				Side = BackFace ? BOTTOM : TOP; 
			}
			else if (d == 2) 
			{ 
				Side = BackFace ? SOUTH : NORTH; 
			}

			// Move through the dimension from front to back
			for (x[d] = -1; x[d] < CHUNK_SIZE;)
			{
				// Compute mask
				n = 0;

				for (x[v] = 0; x[v] < CHUNK_SIZE; x[v]++)
				{
					for (x[u] = 0; x[u] < CHUNK_SIZE; x[u]++)
					{
						// Check covering voxel
						FBlock Voxel1 = (x[d] >= 0) ? mBlocks[BlockIndex(x[0], x[1], x[2])] : FBlock{ FBlock::None };
						FBlock Voxel2 = (x[d] < CHUNK_SIZE - 1) ? mBlocks[BlockIndex(x[0] + q[0], x[1] + q[1], x[2] + q[2])] : FBlock{ FBlock::None };

						// If both voxels are active and the same type, mark the mask with an inactive block, if not
						// choose the appropriate voxel to mark
						if (Voxel1 == Voxel2)
							Mask[n++].Block = FBlock{ FBlock::None };
						else
						{
							int32_t DepthOffset = 0;
							Vector3i BlockPosition;
							
							// Set the correct block in the mask and get the direction we need to check for surrounding
							// blocks for ambient occlusion calculations
							if (BackFace)
							{
								Mask[n].Block = Voxel2;
								DepthOffset = -1;
								BlockPosition = Vector3i{ x[0] + q[0], x[1] + q[1], x[2] + q[2] };
							}
							else
							{
								Mask[n].Block = Voxel1;
								DepthOffset = 1;
								BlockPosition = Vector3i{ x[0], x[1], x[2] };
							}

							// Get status of surrounding blocks for ambient occlusion
							//  ---------
							//  | 0 1 2 | // Surrounding blocks a closer than C
							//  | 3 C 4 | // C = Current
							//  | 5 6 7 |
							//  ---------
							bool Sides[8] = { false, false, false, false, false, false, false, false };

							CheckAOSides(Sides, u, v, d, BlockPosition, DepthOffset);
	
							Mask[n].AOFactors = ComputeBlockFaceAO(Sides);

							n++;
						}
					}
				}

				x[d]++;

				// Generate the mesh for the mask
				n = 0;

				for (j = 0; j < CHUNK_SIZE; j++)
				{
					for (i = 0; i < CHUNK_SIZE;)
					{
						if (Mask[n].Block.Type != FBlock::None)
						{
							// Compute the width
							for (Width = 1; i + Width < CHUNK_SIZE && IsMeshable(Mask[n + Width], Mask[n]); Width++){}

							// Compute Height
							bool Done = false;

							for (Height = 1; j + Height < CHUNK_SIZE; Height++)
							{
								for (k = 0; k < Width; k++)
								{
									if (!IsMeshable(Mask[n + k + Height*CHUNK_SIZE], Mask[n]))
									{
										Done = true;
										break;
									}
								}

								if (Done) 
									break;
							}


							// Add the quad
							x[u] = i;
							x[v] = j;

							du[0] = 0;
							du[1] = 0;
							du[2] = 0;
							du[u] = Width;

							dv[0] = 0;
							dv[1] = 0;
							dv[2] = 0;
							dv[v] = Height;

							mIsEmpty[!mActiveMesh] = false;
							AddQuad(Vector3f(x[0], x[1], x[2]),
								Vector3f(x[0] + du[0], x[1] + du[1], x[2] + du[2]),
								Vector3f(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]),
								Vector3f(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]),
								BackFace,
								Side,
								Mask[n],
								Vertices,
								Indices);


							// Zero the mask
							for (Length = 0; Length < Height; Length++)
							{
								for (k = 0; k < Width; k++)
								{
									Mask[n + k + Length * CHUNK_SIZE].Block.Type = FBlock::None;
								}
							}

							// Increment counters
							i += Width;
							n += Width;
						}
						else
						{
							i++;
							n++;
						}
					}
				}
			}
		}
	}

	// Send all data directly to the gpu
	mMesh[!mActiveMesh]->AddVertex(Vertices.data(), Vertices.size());
	mMesh[!mActiveMesh]->AddIndices(Indices.data(), Indices.size());
}

void FChunk::CheckAOSides(bool Sides[8], int32_t u, int32_t v, int32_t d, Vector3i BlockPosition, int32_t DepthOffset) const
{
	bool HasTop = BlockPosition[v] < (CHUNK_SIZE - 1);
	bool HasBottom = BlockPosition[v] > 0;
	bool HasLeft = BlockPosition[u] > 0;
	bool HasRight = BlockPosition[u] < (CHUNK_SIZE - 1);
	bool HasFront = ((BlockPosition[d] + DepthOffset) <= (CHUNK_SIZE - 1)) && ((BlockPosition[d] + DepthOffset) >= 0);

	//   4       3       2        1      0
	// Front    Left   Right   Bottom   Top
	uint8_t SideMask = (HasFront << 4) + (HasLeft << 3) + (HasRight << 2) + (HasBottom << 1) + HasTop;

	Vector3i Temp = BlockPosition;
	Temp[d] += DepthOffset;

	// If surrounding blocks are all in this chunk
	if (SideMask == 0x1F)
	{
		Temp[u] -= 1; Temp[v] += 1;
		Sides[0] = mBlocks[BlockIndex(Temp)].Type != FBlock::None;

		Temp[u] += 1;
		Sides[1] = mBlocks[BlockIndex(Temp)].Type != FBlock::None;

		Temp[u] += 1;
		Sides[2] = mBlocks[BlockIndex(Temp)].Type != FBlock::None;

		Temp[v] -= 1;
		Sides[4] = mBlocks[BlockIndex(Temp)].Type != FBlock::None;

		Temp[u] -= 2;
		Sides[3] = mBlocks[BlockIndex(Temp)].Type != FBlock::None;

		Temp[v] -= 1;
		Sides[5] = mBlocks[BlockIndex(Temp)].Type != FBlock::None;

		Temp[u] += 1;
		Sides[6] = mBlocks[BlockIndex(Temp)].Type != FBlock::None;

		Temp[u] += 1;
		Sides[7] = mBlocks[BlockIndex(Temp)].Type != FBlock::None;
	}
}

bool FChunk::IsMeshable(const MaskInfo& Lhs, const MaskInfo& Rhs) const
{
	if (Lhs.Block != Rhs.Block || Lhs.Block.Type == FBlock::None || Rhs.Block.Type == FBlock::None)
		return false;

	int32_t BaseAO = Lhs.AOFactors.x;
	for (size_t i = 0; i < 4; i++)
	{
		if (Lhs.AOFactors[i] != BaseAO || Rhs.AOFactors[i] != BaseAO)
			return false;
	}

	return true;
}

Vector4i FChunk::ComputeBlockFaceAO(bool Sides[8]) const
{
	//  ---------
	//  | 0 1 2 | // Surrounding blocks a closer than C
	//  | 3 C 4 | // C = Current
	//  | 5 6 7 |
	//  ---------

	Vector4i FaceAO{0,0,0,0};
	FaceAO.x = ComputeBlockVertexAO(Sides[3], Sides[1], Sides[0]); // Top left vertex
	FaceAO.y = ComputeBlockVertexAO(Sides[3], Sides[6], Sides[5]); // Bottom left vertex
	FaceAO.z = ComputeBlockVertexAO(Sides[4], Sides[6], Sides[7]); // Bottom right vertex
	FaceAO.w = ComputeBlockVertexAO(Sides[1], Sides[4], Sides[2]); // Top right vertex
	return FaceAO;
}

int32_t FChunk::ComputeBlockVertexAO(bool Side1, bool Side2, bool Corner) const
{
	if (Side1 && Side2)
		return 0;

	return 3 - (Side1 + Side2 + Corner);
}

void FChunk::AddQuad(	const Vector3f BottomLeft,
						const Vector3f BottomRight,
						const Vector3f TopRight,
						const Vector3f TopLeft,
						bool IsBackface,
						uint32_t Side,
						const MaskInfo& FaceInfo,
						std::vector<FVoxelVertex>& VerticesOut,
						std::vector<uint32_t>& IndicesOut)
{
	Vector3f Normal;
	switch (Side)
	{
	case WEST:
		Normal = Vector3f(-1, 0, 0);
		break;
	case EAST:
		Normal = Vector3f(1, 0, 0);
		break;
	case NORTH:
		Normal = Vector3f(0, 0, 1);
		break;
	case SOUTH:
		Normal = Vector3f(0, 0, -1);
		break;
	case TOP:
		Normal = Vector3f(0, 1, 0);
		break;
	case BOTTOM:
		Normal = Vector3f(0, -1, 0);
		break;
	}
	
	// Get the index offset by checking the size of the vertex list.
	static const float AOFactors[4] = { 0.4f, 0.65f, 0.85f, 1.0f };
	const Vector4f ComputedAOFactors{ AOFactors[FaceInfo.AOFactors.x], AOFactors[FaceInfo.AOFactors.y], AOFactors[FaceInfo.AOFactors.z], AOFactors[FaceInfo.AOFactors.w] };
	uint32_t BaseIndex = VerticesOut.size();

	// We need to rearrange the order that the quad is constructed based on AO factors for each vertex. This prevents anisotropy.
	if (ComputedAOFactors.w + ComputedAOFactors.y > ComputedAOFactors.x + ComputedAOFactors.z)
	{
		VerticesOut.insert(VerticesOut.end(), { { Vector4f{ BottomLeft, ComputedAOFactors.y }, Normal, FBlock::Colors[FaceInfo.Block.Type] },
												{ Vector4f{ BottomRight, ComputedAOFactors.z }, Normal, FBlock::Colors[FaceInfo.Block.Type] },
												{ Vector4f{ TopRight, ComputedAOFactors.w }, Normal, FBlock::Colors[FaceInfo.Block.Type] },
												{ Vector4f{ TopLeft, ComputedAOFactors.x }, Normal, FBlock::Colors[FaceInfo.Block.Type] } });
	}
	else
	{
		VerticesOut.insert(VerticesOut.end(), { { Vector4f{ TopLeft, ComputedAOFactors.x }, Normal, FBlock::Colors[FaceInfo.Block.Type] },
												{ Vector4f{ BottomLeft, ComputedAOFactors.y }, Normal, FBlock::Colors[FaceInfo.Block.Type] },
												{ Vector4f{ BottomRight, ComputedAOFactors.z }, Normal, FBlock::Colors[FaceInfo.Block.Type] },
												{ Vector4f{ TopRight, ComputedAOFactors.w }, Normal, FBlock::Colors[FaceInfo.Block.Type] } });
	}

	// Add adjusted indices.
	if (!IsBackface)
	{
		IndicesOut.insert(IndicesOut.end(), {	0 + BaseIndex, 
												1 + BaseIndex, 
												2 + BaseIndex, 
												2 + BaseIndex, 
												3 + BaseIndex, 
												0 + BaseIndex });

	}
	else
	{
		IndicesOut.insert(IndicesOut.end(), {	0 + BaseIndex, 
												3 + BaseIndex, 
												2 + BaseIndex, 
												0 + BaseIndex, 
												2 + BaseIndex, 
												1 + BaseIndex });
	}

}