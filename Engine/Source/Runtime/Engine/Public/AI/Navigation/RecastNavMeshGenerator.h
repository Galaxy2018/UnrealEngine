// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#if WITH_RECAST

#include "Recast.h"
#include "DetourNavMesh.h"
#include "AI/NavDataGenerator.h"
#include "AI/NavigationModifier.h"

#define MAX_VERTS_PER_POLY	6

class FNavigationOctree;
class FNavMeshBuildContext;
class FRecastNavMeshGenerator;
struct FNavigationRelevantData;
struct dtTileCacheLayer;

struct FRecastBuildConfig : public rcConfig
{
	/** controls whether voxel filterring will be applied (via FRecastTileGenerator::ApplyVoxelFilter) */
	uint32 bPerformVoxelFiltering:1;
	/** generate detailed mesh (additional tessellation to match heights of geometry) */
	uint32 bGenerateDetailedMesh:1;
	/** generate BV tree (space partitioning for queries) */
	uint32 bGenerateBVTree:1;
	/** if set, mark areas with insufficient free height instead of cutting them out  */
	uint32 bMarkLowHeightAreas : 1;

	/** region partitioning method used by tile cache */
	int32 TileCachePartitionType;
	/** chunk size for ChunkyMonotone partitioning */
	int32 TileCacheChunkSize;

	int32 PolyMaxHeight;
	/** indicates what's the limit of navmesh polygons per tile. This value is calculated from other
	 *	factors - DO NOT SET IT TO ARBITRARY VALUE */
	int32 MaxPolysPerTile;

	/** Actual agent height (in uu)*/
	float AgentHeight;
	/** Actual agent climb (in uu)*/
	float AgentMaxClimb;
	/** Actual agent radius (in uu)*/
	float AgentRadius;
	/** Agent index for filtering links */
	int32 AgentIndex;

	FRecastBuildConfig()
	{
		Reset();
	}

	void Reset()
	{
		FMemory::MemZero(*this);
		bPerformVoxelFiltering = true;
		bGenerateDetailedMesh = true;
		bGenerateBVTree = true;
		bMarkLowHeightAreas = false;
		PolyMaxHeight = 10;
		MaxPolysPerTile = -1;
		AgentIndex = 0;
	}
};

struct FRecastVoxelCache
{
	struct FTileInfo
	{
		int16 TileX;
		int16 TileY;
		int32 NumSpans;
		FTileInfo* NextTile;
		rcSpanCache* SpanData;
	};

	int32 NumTiles;

	/** tile info */
	FTileInfo* Tiles;

	FRecastVoxelCache() {}
	FRecastVoxelCache(const uint8* Memory);
};

struct FRecastGeometryCache
{
	struct FHeader
	{
		int32 NumVerts;
		int32 NumFaces;
		struct FWalkableSlopeOverride SlopeOverride;
	};

	FHeader Header;

	/** recast coords of vertices (size: NumVerts * 3) */
	float* Verts;

	/** vert indices for triangles (size: NumFaces * 3) */
	int32* Indices;

	FRecastGeometryCache() {}
	FRecastGeometryCache(const uint8* Memory);
};

/**
 * Class handling generation of a single tile, caching data that can speed up subsequent tile generations
 */
class ENGINE_API FRecastTileGenerator : public FNonAbandonableTask
{
public:
	FRecastTileGenerator(
		const FRecastNavMeshGenerator* ParentGenerator,
		const int32 X, 
		const int32 Y,
		TArray<FBox> DirtyAreas
	);
		
	void DoWork();
	static const TCHAR *Name();

	FORCEINLINE int32 GetTileX() const { return TileX; }
	FORCEINLINE int32 GetTileY() const { return TileY; }
	/** Whether specified layer was updated */
	FORCEINLINE bool IsLayerChanged(int32 LayerIdx) const { return DirtyLayers[LayerIdx]; }
	/** Whether tile data was fully regenerated */
	FORCEINLINE bool IsFullyRegenerated() const { return bRegenerateCompressedLayers; }

	TArray<FNavMeshTileData> GetCompressedLayers() const { return CompressedLayers; }
	TArray<FNavMeshTileData> GetNavigationData() const { return NavigationData; }
	
	uint32 GetUsedMemCount() const;

	// Memory amount used to construct generator 
	uint32 UsedMemoryOnStartup;
	
private:
	/** Does the actual tile generations. 
	 *	@note always trigger tile generation only via TriggerAsyncBuild. This is a worker function
	 *	@return true if new tile navigation data has been generated and is ready to be added to navmesh instance, 
	 *	false if failed or no need to generate (still valid)
	 */
	bool GenerateTile();
	
	void GatherGeometry(const FRecastNavMeshGenerator* ParentGenerator, bool bGeometryChanged);

	/** builds CompressedLayers array (geometry + modifiers) */
	bool GenerateCompressedLayers(FNavMeshBuildContext& BuildContext);

	/** builds NavigationData array (layers + obstacles) */
	bool GenerateNavigationData(FNavMeshBuildContext& BuildContext);

	void ApplyVoxelFilter(struct rcHeightfield* SolidHF, float WalkableRadius);

	/** apply areas from StaticAreas to heightfield */
	void MarkStaticAreas(FNavMeshBuildContext& BuildContext, rcCompactHeightfield& CompactHF);

	/** apply areas from DynamicAreas to layer */
	void MarkDynamicAreas(dtTileCacheLayer& Layer);
	
	void AppendModifier(const FCompositeNavModifier& Modifier, bool bStatic);
	/** Appends specified geometry to tile's geometry */
	void AppendGeometry(const TNavStatArray<uint8>& RawCollisionCache);
	void AppendGeometry(const TNavStatArray<FVector>& Verts, const TNavStatArray<int32>& Faces);
	void AppendVoxels(rcSpanCache* SpanData, int32 NumSpans);
	
	/** prepare voxel cache from collision data */
	void PrepareVoxelCache(const TNavStatArray<uint8>& RawCollisionCache, TNavStatArray<rcSpanCache>& SpanData);
	bool HasVoxelCache(const TNavStatArray<uint8>& RawVoxelCache, rcSpanCache*& CachedVoxels, int32& NumCachedVoxels) const;
	void AddVoxelCache(TNavStatArray<uint8>& RawVoxelCache, const rcSpanCache* CachedVoxels, const int32 NumCachedVoxels) const;

protected:
	uint32 bSucceeded : 1;
	uint32 bRegenerateCompressedLayers : 1;
	uint32 bFullyEncapsulatedByInclusionBounds : 1;
	
	int32 TileX;
	int32 TileY;
	uint32 Version;
	/** Tile's bounding box, Unreal coords */
	FBox TileBB;
	
	/** Layers dirty flags */
	TBitArray<>  DirtyLayers;
	
	/** Parameters defining navmesh tiles */
	FRecastBuildConfig TileConfig;

	/** Bounding geometry definition. */
	TNavStatArray<FBox> InclusionBounds;

	/** Additional config */
	FRecastNavMeshCachedData AdditionalCachedData;

	// generated tile data
	TArray<FNavMeshTileData> CompressedLayers;
	TArray<FNavMeshTileData> NavigationData;
	
	// tile's geometry: without voxel cache
	TArray<float> GeomCoords;
	TArray<int32> GeomIndices;
	// areas used for creating compressed layers: static zones
	TArray<FAreaNavModifier> StaticAreas;
	// areas used for creating navigation data: obstacles
	TArray<FAreaNavModifier> DynamicAreas;
	// navigation links
	TArray<FSimpleLinkNavModifier> OffmeshLinks;
};

typedef FAsyncTask<FRecastTileGenerator> FRecastTileGeneratorTask;

struct FTileElement
{
	/** tile coordinates on a grid in recast space */
	FIntPoint	Coord;
	/** distance to seed, used for sorting pending tiles */
	float		SeedDistance; 
	/** generator task associated with this tile */
	FRecastTileGeneratorTask* AsyncTask;
	/** Whether we need a full rebuild for this tile grid cell */
	bool		bRebuildGeometry;
	/** We need to store dirty area bounds to check which cached layers needs to be regenerated
	 *  In case geometry is changed cached layers data will be fully regenerated without using dirty areas list
	 */
	TArray<FBox> DirtyAreas;
	
	FTileElement()
		: Coord(FIntPoint::NoneValue)
		, SeedDistance(MAX_flt)
		, AsyncTask(nullptr)
		, bRebuildGeometry(false)
	{
	}

	bool operator == (const FTileElement& Other) const
	{
		return Coord == Other.Coord;
	}

	bool operator < (const FTileElement& Other) const
	{
		return Other.SeedDistance < SeedDistance;
	}

	friend uint32 GetTypeHash(const FTileElement& Element)
	{
		return GetTypeHash(Element.Coord);
	}
};

struct FTileTimestamp
{
	uint32 TileIdx;
	double Timestamp;
	
	bool operator == (const FTileTimestamp& Other) const
	{
		return TileIdx == Other.TileIdx;
	}
};

/**
 * Class that handles generation of the whole Recast-based navmesh.
 */
class FRecastNavMeshGenerator : public FNavDataGenerator
{
public:
	FRecastNavMeshGenerator(class ARecastNavMesh* InDestNavMesh);
	virtual ~FRecastNavMeshGenerator();

private:
	/** Prevent copying. */
	FRecastNavMeshGenerator(FRecastNavMeshGenerator const& NoCopy) { check(0); };
	FRecastNavMeshGenerator& operator=(FRecastNavMeshGenerator const& NoCopy) { check(0); return *this; }

public:
	virtual bool RebuildAll() override;
	virtual void EnsureBuildCompletion() override;
	virtual void CancelBuild() override;
	virtual void TickAsyncBuild(float DeltaSeconds) override;
	virtual void OnNavigationBoundsChanged() override;

	/** Asks generator to update navigation affected by DirtyAreas */
	virtual void RebuildDirtyAreas(const TArray<FNavigationDirtyArea>& DirtyAreas) override;
	virtual bool IsBuildInProgress(bool bCheckDirtyToo = false) const override;
	virtual int32 GetNumRemaningBuildTasks() const override;
	virtual int32 GetNumRunningBuildTasks() const override;

	/** Checks if a given tile is being build or has just finished building
	 *	If TileIndex is out of bounds function returns false (i.e. not being built) */
	bool IsTileChanged(int32 TileIdx) const;
		
	FORCEINLINE uint32 GetVersion() const { return Version; }

	const ARecastNavMesh* GetOwner() const { return DestNavMesh; }

	/** update area data */
	void OnAreaAdded(const UClass* AreaClass, int32 AreaID);
		
	//--- accessors --- //
	FORCEINLINE class UWorld* GetWorld() const { return DestNavMesh->GetWorld(); }

	const FRecastBuildConfig& GetConfig() const { return Config; }

	const TNavStatArray<FBox>& GetInclusionBounds() const { return InclusionBounds; }
	
	/** Whether specified box in inside or intersects with nav bounds  */
	bool IntercestsInclusionBounds(const FBox& Box) const;

	/** Total navigable area box, sum of all navigation volumes bounding boxes */
	FBox GetTotalBounds() const { return TotalNavBounds; }

	const FRecastNavMeshCachedData& GetAdditionalCachedData() const { return AdditionalCachedData; }

	TArray<uint32> AddTile(const FRecastTileGenerator& TileGenerator);
	bool HasDirtyTiles() const;

	FBox GrowBoundingBox(const FBox& BBox, bool bIncludeAgentHeight) const;

	/**  */
	TArray<FNavMeshTileData> GetCachedLayersData(FIntPoint GridCoord) const;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual void ExportNavigationData(const FString& FileName) const;
#endif

	/** 
	 *	@param Actor is a reference to make callee responsible for assuring it's valid
	 */
	static void ExportComponentGeometry(UActorComponent* Component, FNavigationRelevantData& Data);
	static void ExportVertexSoupGeometry(const TArray<FVector>& Verts, FNavigationRelevantData& Data);

	static void ExportRigidBodyGeometry(UBodySetup& BodySetup, TNavStatArray<FVector>& OutVertexBuffer, TNavStatArray<int32>& OutIndexBuffer, const FTransform& LocalToWorld = FTransform::Identity);
	static void ExportRigidBodyGeometry(UBodySetup& BodySetup, TNavStatArray<FVector>& OutTriMeshVertexBuffer, TNavStatArray<int32>& OutTriMeshIndexBuffer, TNavStatArray<FVector>& OutConvexVertexBuffer, TNavStatArray<int32>& OutConvexIndexBuffer, TNavStatArray<int32>& OutShapeBuffer, const FTransform& LocalToWorld = FTransform::Identity);

private:
	// Performs initial setup of member variables so that generator is ready to do its thing from this point on
	void Init();

	// Updates cached list of navigation bounds
	void UpdateNavigationBounds();
		
	// Sorts pending build tiles by proximity to player, so tiles closer to player will get generated first
	void SortPendingBuildTiles();

	/** Instantiates dtNavMesh and configures it for tiles generation. Returns false if failed */
	bool ConstructTiledNavMesh();
	
	/** Marks grid tiles affected by specified areas as dirty */
	void MarkDirtyTiles(const TArray<FNavigationDirtyArea>& DirtyAreas);
	
	/** Submits up to specified number of async generator tasks for grid tiles that was marked as dirty */
	int32 SubmitDirtyTiles(const int32 NumTasksToSubmit);
	
	/** Collect completed async tasks and applies new tile data to a navmesh*/
	TArray<uint32> CollectCompletedTiles();

	/** Removes all tiles at specified grid location */
	TArray<uint32> RemoveTileLayers(const int32 TileX, const int32 TileY);
	
	/** Blocks until build for specified list of tiles is complete and discard results */
	void DiscardCurrentBuildingTasks();

	//----------------------------------------------------------------------//
	// debug
	//----------------------------------------------------------------------//
	virtual uint32 LogMemUsed() const override;

private:
	/** Parameters defining navmesh tiles */
	struct dtNavMeshParams TiledMeshParams;
	struct FRecastBuildConfig Config;
	
	int32 NumActiveTiles;
	int32 MaxTileGeneratorTasks;
	float AvgLayersPerTile;

	/** Total bounding box that includes all volumes, in unreal units. */
	FBox TotalNavBounds;

	/** Bounding geometry definition. */
	TNavStatArray<FBox> InclusionBounds;

	/** Navigation mesh that owns this generator */
	ARecastNavMesh*	DestNavMesh;
	
	/** List of dirty tiles that needs to be regenerated */
	TNavStatArray<FTileElement> PendingDirtyTiles;			
	
	/** List of dirty tiles currently being regenerated */
	TNavStatArray<FTileElement> RunningDirtyTiles;

#if WITH_EDITOR
	/** List of tiles that were recently regenerated */
	TNavStatArray<FTileTimestamp> RecentlyBuiltTiles;
#endif// WITH_EDITOR

	/** */
	FRecastNavMeshCachedData AdditionalCachedData;

	/** Compressed layers data, can be reused for tiles generation */
	TMap<FIntPoint, TArray<FNavMeshTileData>> CachedLayerDataMap;

	/** */
	TMapBase<const AActor*, FBox, false> ActorToAreaMap;

	uint32 bInitialized:1;

	/** Runtime generator's version, increased every time all tile generators get invalidated
	 *	like when navmesh size changes */
	uint32 Version;
};

#endif // WITH_RECAST
