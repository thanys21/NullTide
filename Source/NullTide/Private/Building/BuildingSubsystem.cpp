#include "Building/BuildingSubsystem.h"

#include "Building/BuildingDefinition.h"
#include "Building/BuildingPieceComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
FBuildingPlacementResult MakeFailure(FBuildingPlacementResult Result, const EBuildResult Code, const FString& Diagnostic,
	AActor* BlockingActor = nullptr)
{
	Result.Result = Code;
	Result.Diagnostic = Diagnostic;
	Result.BlockingActor = BlockingActor;
	return Result;
}

/** Whether a trace hit an Active Foundation piece. */
bool IsActiveFloorHit(const FHitResult& Hit)
{
	const AActor* Actor = Hit.GetActor();
	const UBuildingPieceComponent* Piece = Actor ? Actor->FindComponentByClass<UBuildingPieceComponent>() : nullptr;
	const UBuildingDefinition* Definition = Piece && Piece->GetBuildingDefinition()
		? Piece->GetBuildingDefinition()->GetDefaultObject<UBuildingDefinition>()
		: nullptr;
	return Piece && Piece->IsActivePiece() && Definition && Definition->Category == EBuildingCategory::Foundation;
}

/** A trace hit that may carry a piece: Active floors are valid support, every other solid actor is not. */
bool IsSupportSurface(const FHitResult& Hit)
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Component)
	{
		return false;
	}
	if (Component->GetCollisionObjectType() == ECC_WorldStatic)
	{
		return true;
	}
	return IsActiveFloorHit(Hit);
}
}

FString FBuildingOccupancyKey::ToString() const
{
	return FString::Printf(TEXT("%s/%s(%d,%d)L%d"),
		*StaticEnum<EBuildingOccupancyCategory>()->GetNameStringByValue(static_cast<int64>(Category)),
		*StaticEnum<EBuildingEdgeAxis>()->GetNameStringByValue(static_cast<int64>(Axis)), X, Y, Layer);
}

UBuildingSubsystem* UBuildingSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBuildingSubsystem>() : nullptr;
}

// ---- Grid math ----

FIntPoint UBuildingSubsystem::WorldToCell(const FVector& WorldLocation)
{
	return FIntPoint(FMath::FloorToInt32(WorldLocation.X / CellSize), FMath::FloorToInt32(WorldLocation.Y / CellSize));
}

FVector UBuildingSubsystem::CellToWorldCenter(const FIntPoint& Cell, const double Z)
{
	return FVector((Cell.X + 0.5) * CellSize, (Cell.Y + 0.5) * CellSize, Z);
}

void UBuildingSubsystem::SnapToEdge(const FVector& WorldLocation, EBuildingEdgeAxis& OutAxis, FIntPoint& OutEdge)
{
	const FIntPoint Cell = WorldToCell(WorldLocation);
	const double U = WorldLocation.X / CellSize - Cell.X;
	const double V = WorldLocation.Y / CellSize - Cell.Y;
	// Fixed tie-break order: South, West, North, East (a strictly smaller distance is needed to move on).
	const double Distances[4] = { V, U, 1.0 - V, 1.0 - U };
	int32 Best = 0;
	for (int32 Side = 1; Side < 4; ++Side)
	{
		if (Distances[Side] < Distances[Best])
		{
			Best = Side;
		}
	}
	switch (Best)
	{
	case 0: OutAxis = EBuildingEdgeAxis::X; OutEdge = FIntPoint(Cell.X, Cell.Y); break;     // South
	case 1: OutAxis = EBuildingEdgeAxis::Y; OutEdge = FIntPoint(Cell.X, Cell.Y); break;     // West
	case 2: OutAxis = EBuildingEdgeAxis::X; OutEdge = FIntPoint(Cell.X, Cell.Y + 1); break; // North
	default: OutAxis = EBuildingEdgeAxis::Y; OutEdge = FIntPoint(Cell.X + 1, Cell.Y); break; // East
	}
}

FVector UBuildingSubsystem::EdgeToWorldCenter(const EBuildingEdgeAxis Axis, const FIntPoint& Edge, const double Z)
{
	return Axis == EBuildingEdgeAxis::Y
		? FVector(Edge.X * CellSize, (Edge.Y + 0.5) * CellSize, Z)
		: FVector((Edge.X + 0.5) * CellSize, Edge.Y * CellSize, Z);
}

int32 UBuildingSubsystem::NormalizeRotation(const int32 Rotation)
{
	return ((Rotation % 4) + 4) % 4;
}

FIntPoint UBuildingSubsystem::GetRotatedFootprint(const FIntPoint& Footprint, const int32 Rotation)
{
	return NormalizeRotation(Rotation) % 2 == 1 ? FIntPoint(Footprint.Y, Footprint.X) : Footprint;
}

int32 UBuildingSubsystem::HeightToLayer(const double SurfaceZ)
{
	return FMath::RoundToInt32(SurfaceZ / LayerHeight);
}

double UBuildingSubsystem::GetPlacementYaw(const FBuildingPlacement& Placement)
{
	const int32 Rotation = NormalizeRotation(Placement.Rotation);
	if (Placement.PlacementMode == EBuildingPlacementMode::GridEdge)
	{
		const double Base = Placement.EdgeAxis == EBuildingEdgeAxis::Y ? 90.0 : 0.0;
		return FMath::Fmod(Base + (Rotation % 2 == 1 ? 180.0 : 0.0), 360.0);
	}
	return Rotation * 90.0;
}

FBuildingPlacement UBuildingSubsystem::MakePlacementFromWorld(const UBuildingDefinition& Definition, const FVector& WorldLocation,
	const int32 Rotation)
{
	FBuildingPlacement Placement;
	Placement.PlacementMode = Definition.PlacementMode;
	Placement.Rotation = NormalizeRotation(Rotation);
	Placement.SurfaceZ = WorldLocation.Z;
	if (Definition.PlacementMode == EBuildingPlacementMode::GridEdge)
	{
		SnapToEdge(WorldLocation, Placement.EdgeAxis, Placement.Anchor);
	}
	else
	{
		Placement.EdgeAxis = EBuildingEdgeAxis::None;
		Placement.Anchor = WorldToCell(WorldLocation);
	}
	Placement.Layer = HeightToLayer(Placement.SurfaceZ);
	Placement.WorldTransform = ComputeWorldTransform(Definition, Placement);
	return Placement;
}

TArray<FBuildingOccupancyKey> UBuildingSubsystem::ComputeOccupancyKeys(const UBuildingDefinition& Definition,
	const FBuildingPlacement& Placement)
{
	TArray<FBuildingOccupancyKey> Keys;
	if (Placement.PlacementMode == EBuildingPlacementMode::GridEdge)
	{
		Keys.Emplace(EBuildingOccupancyCategory::Edge, Placement.EdgeAxis, Placement.Anchor.X, Placement.Anchor.Y, Placement.Layer);
		return Keys;
	}
	const FIntPoint Size = GetRotatedFootprint(Definition.Footprint, Placement.Rotation);
	for (int32 DY = 0; DY < Size.Y; ++DY)
	{
		for (int32 DX = 0; DX < Size.X; ++DX)
		{
			Keys.Emplace(Definition.GetOccupancyCategory(), EBuildingEdgeAxis::None,
				Placement.Anchor.X + DX, Placement.Anchor.Y + DY, Placement.Layer);
		}
	}
	return Keys;
}

FTransform UBuildingSubsystem::ComputeWorldTransform(const UBuildingDefinition& Definition, const FBuildingPlacement& Placement)
{
	FVector Location;
	if (Placement.PlacementMode == EBuildingPlacementMode::GridEdge)
	{
		Location = EdgeToWorldCenter(Placement.EdgeAxis, Placement.Anchor, Placement.SurfaceZ);
	}
	else
	{
		const FIntPoint Size = GetRotatedFootprint(Definition.Footprint, Placement.Rotation);
		Location = FVector((Placement.Anchor.X + Size.X * 0.5) * CellSize, (Placement.Anchor.Y + Size.Y * 0.5) * CellSize,
			Placement.SurfaceZ);
	}
	return FTransform(FRotator(0.0, GetPlacementYaw(Placement), 0.0), Location);
}

TArray<FVector2D> UBuildingSubsystem::GetSurfaceSamplePoints(const UBuildingDefinition& Definition, const FBuildingPlacement& Placement)
{
	TArray<FVector2D> Samples;
	if (Placement.PlacementMode == EBuildingPlacementMode::GridEdge)
	{
		// Both ends (inset past a corner neighbour's half thickness) and the midpoint, on both sides of the edge
		// line, so a wall standing on a floor's boundary sees that floor.
		const double Side = WallThickness * 0.5 - CollisionClearance;
		const double Along[3] = { EdgeEndInset, CellSize * 0.5, CellSize - EdgeEndInset };
		for (const double T : Along)
		{
			for (const double Offset : { -Side, Side })
			{
				Samples.Add(Placement.EdgeAxis == EBuildingEdgeAxis::Y
					? FVector2D(Placement.Anchor.X * CellSize + Offset, Placement.Anchor.Y * CellSize + T)
					: FVector2D(Placement.Anchor.X * CellSize + T, Placement.Anchor.Y * CellSize + Offset));
			}
		}
		return Samples;
	}
	const FIntPoint Size = GetRotatedFootprint(Definition.Footprint, Placement.Rotation);
	// Corners inset past a neighbouring wall's half thickness, plus the center.
	const double MinX = Placement.Anchor.X * CellSize + EdgeEndInset;
	const double MinY = Placement.Anchor.Y * CellSize + EdgeEndInset;
	const double MaxX = (Placement.Anchor.X + Size.X) * CellSize - EdgeEndInset;
	const double MaxY = (Placement.Anchor.Y + Size.Y) * CellSize - EdgeEndInset;
	Samples.Add(FVector2D(MinX, MinY));
	Samples.Add(FVector2D(MaxX, MinY));
	Samples.Add(FVector2D(MinX, MaxY));
	Samples.Add(FVector2D(MaxX, MaxY));
	Samples.Add(FVector2D((MinX + MaxX) * 0.5, (MinY + MaxY) * 0.5));
	return Samples;
}

FBox UBuildingSubsystem::GetCollisionBox(const UBuildingDefinition& Definition, const FBuildingPlacement& Placement)
{
	// The box starts 1 cm above the support surface, so the ground or floor the piece stands on never overlaps it;
	// everything else inside the volume does. No actor is excluded by identity.
	const double Bottom = Placement.SurfaceZ + 1.0;
	if (Placement.PlacementMode == EBuildingPlacementMode::GridEdge)
	{
		// Shorter than the edge by half a wall thickness at each end, so walls meeting at a grid corner do not block.
		const double HalfLength = CellSize * 0.5 - EdgeEndInset;
		const double HalfThickness = WallThickness * 0.5 - CollisionClearance;
		const FVector Center = EdgeToWorldCenter(Placement.EdgeAxis, Placement.Anchor, 0.0);
		const FVector HalfExtent = Placement.EdgeAxis == EBuildingEdgeAxis::Y
			? FVector(HalfThickness, HalfLength, 0.0)
			: FVector(HalfLength, HalfThickness, 0.0);
		return FBox(FVector(Center.X - HalfExtent.X, Center.Y - HalfExtent.Y, Bottom),
			FVector(Center.X + HalfExtent.X, Center.Y + HalfExtent.Y, Placement.SurfaceZ + WallHeight));
	}
	const FIntPoint Size = GetRotatedFootprint(Definition.Footprint, Placement.Rotation);
	const double Height = Definition.Category == EBuildingCategory::Foundation ? FloorThickness : WallHeight;
	// Shrunk past a neighbouring wall's half thickness, so walls and doors on the cell's edges do not block it.
	return FBox(
		FVector(Placement.Anchor.X * CellSize + EdgeEndInset, Placement.Anchor.Y * CellSize + EdgeEndInset, Bottom),
		FVector((Placement.Anchor.X + Size.X) * CellSize - EdgeEndInset, (Placement.Anchor.Y + Size.Y) * CellSize - EdgeEndInset,
			Placement.SurfaceZ + Height));
}

// ---- Occupancy registry ----

bool UBuildingSubsystem::HasDuplicateKeys(const TArray<FBuildingOccupancyKey>& Keys)
{
	TSet<FBuildingOccupancyKey> Seen;
	for (const FBuildingOccupancyKey& Key : Keys)
	{
		bool bAlreadyInSet = false;
		Seen.Add(Key, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			return true;
		}
	}
	return false;
}

bool UBuildingSubsystem::SameKeySet(const TArray<FBuildingOccupancyKey>& A, const TArray<FBuildingOccupancyKey>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}
	const TSet<FBuildingOccupancyKey> SetA(A);
	for (const FBuildingOccupancyKey& Key : B)
	{
		if (!SetA.Contains(Key))
		{
			return false;
		}
	}
	return SetA.Num() == A.Num();
}

bool UBuildingSubsystem::IsOccupantAlive(const FOccupant& Occupant) const
{
	if (Occupant.bReserved)
	{
		const FReservation* Reservation = Reservations.Find(Occupant.ReservationId);
		return Reservation && Reservation->Owner.IsValid();
	}
	const FPieceRecord* Record = Pieces.Find(Occupant.HolderKey);
	return Record && Record->Piece.IsValid();
}

bool UBuildingSubsystem::IsKeyFree(const FBuildingOccupancyKey& Key, const FBuildingReservationHandle& IgnoreReservation) const
{
	const FOccupant* Occupant = Occupancy.Find(Key);
	if (!Occupant || !IsOccupantAlive(*Occupant))
	{
		return true;
	}
	return Occupant->bReserved && IgnoreReservation.IsValid() && Occupant->ReservationId == IgnoreReservation.Id;
}

UBuildingPieceComponent* UBuildingSubsystem::GetOccupyingPiece(const FBuildingOccupancyKey& Key) const
{
	const FOccupant* Occupant = Occupancy.Find(Key);
	if (!Occupant || Occupant->bReserved || !IsOccupantAlive(*Occupant))
	{
		return nullptr;
	}
	return const_cast<UBuildingPieceComponent*>(Cast<UBuildingPieceComponent>(Occupant->Holder.Get()));
}

TArray<UBuildingPieceComponent*> UBuildingSubsystem::FindDependentEdgePieces(const UBuildingPieceComponent& Floor) const
{
	TArray<UBuildingPieceComponent*> Dependents;
	const TArray<FBuildingOccupancyKey> FloorKeys = Floor.GetOccupancyKeys();
	auto IsOtherActiveFloorAt = [this, &Floor](const int32 X, const int32 Y, const int32 Layer)
	{
		const UBuildingPieceComponent* Other = GetOccupyingPiece(
			FBuildingOccupancyKey(EBuildingOccupancyCategory::Floor, EBuildingEdgeAxis::None, X, Y, Layer));
		const UBuildingDefinition* Definition = Other && Other->GetBuildingDefinition()
			? Other->GetBuildingDefinition()->GetDefaultObject<UBuildingDefinition>() : nullptr;
		return Other && Other != &Floor && Other->IsActivePiece() && Definition && Definition->Category == EBuildingCategory::Foundation;
	};
	// A wall's layer comes from the highest floor top beside it, so a floor on layer L carries walls on L or, when
	// its top rounds up, on L + 1; a wall on layer W can stand on a floor of layer W - 1 or W.
	auto IsOtherFloorUnder = [&IsOtherActiveFloorAt](const int32 X, const int32 Y, const int32 WallLayer)
	{
		return IsOtherActiveFloorAt(X, Y, WallLayer) || IsOtherActiveFloorAt(X, Y, WallLayer - 1);
	};
	const int32 TopLayer = HeightToLayer(Floor.GetPlacement().SurfaceZ + FloorThickness);
	for (const FBuildingOccupancyKey& Cell : FloorKeys)
	{
		if (Cell.Category != EBuildingOccupancyCategory::Floor)
		{
			continue;
		}
		TArray<int32, TInlineAllocator<2>> WallLayers = { Cell.Layer };
		WallLayers.AddUnique(TopLayer);
		// The cell's four edges and the cell across each of them.
		struct FEdgeAcross { EBuildingEdgeAxis Axis; int32 EdgeX; int32 EdgeY; int32 CellX; int32 CellY; };
		const FEdgeAcross Edges[4] = {
			{ EBuildingEdgeAxis::X, Cell.X, Cell.Y, Cell.X, Cell.Y - 1 },         // south
			{ EBuildingEdgeAxis::X, Cell.X, Cell.Y + 1, Cell.X, Cell.Y + 1 },     // north
			{ EBuildingEdgeAxis::Y, Cell.X, Cell.Y, Cell.X - 1, Cell.Y },         // west
			{ EBuildingEdgeAxis::Y, Cell.X + 1, Cell.Y, Cell.X + 1, Cell.Y },     // east
		};
		for (const FEdgeAcross& Edge : Edges)
		{
			for (const int32 WallLayer : WallLayers)
			{
				UBuildingPieceComponent* EdgePiece = GetOccupyingPiece(
					FBuildingOccupancyKey(EBuildingOccupancyCategory::Edge, Edge.Axis, Edge.EdgeX, Edge.EdgeY, WallLayer));
				const UBuildingDefinition* Definition = EdgePiece && EdgePiece->GetBuildingDefinition()
					? EdgePiece->GetBuildingDefinition()->GetDefaultObject<UBuildingDefinition>() : nullptr;
				if (Definition && Definition->bRequiresFloor && !IsOtherFloorUnder(Edge.CellX, Edge.CellY, WallLayer))
				{
					Dependents.AddUnique(EdgePiece);
				}
			}
		}
	}
	return Dependents;
}

bool UBuildingSubsystem::IsPieceRegistered(const UBuildingPieceComponent* Piece) const
{
	return Piece && Pieces.Contains(FObjectKey(Piece));
}

bool UBuildingSubsystem::IsReservationActive(const FBuildingReservationHandle& Handle) const
{
	const FReservation* Reservation = Handle.IsValid() ? Reservations.Find(Handle.Id) : nullptr;
	return Reservation && Reservation->Owner.IsValid();
}

int32 UBuildingSubsystem::GetOccupiedKeyCount() const
{
	int32 Count = 0;
	for (const TPair<FBuildingOccupancyKey, FOccupant>& Entry : Occupancy)
	{
		Count += !Entry.Value.bReserved ? 1 : 0;
	}
	return Count;
}

int32 UBuildingSubsystem::GetReservedKeyCount() const
{
	return Occupancy.Num() - GetOccupiedKeyCount();
}

FBuildingReservationHandle UBuildingSubsystem::ReserveKeys(const UObject* Owner, const TArray<FBuildingOccupancyKey>& Keys)
{
	if (!IsValid(Owner) || Keys.IsEmpty() || HasDuplicateKeys(Keys))
	{
		return FBuildingReservationHandle();
	}
	PurgeStaleEntries();
	for (const FBuildingOccupancyKey& Key : Keys)
	{
		if (!IsKeyFree(Key))
		{
			return FBuildingReservationHandle();
		}
	}
	FBuildingReservationHandle Handle;
	Handle.Id = NextReservationId++;
	FReservation& Reservation = Reservations.Add(Handle.Id);
	Reservation.OwnerKey = FObjectKey(Owner);
	Reservation.Owner = Owner;
	Reservation.Keys = Keys;
	for (const FBuildingOccupancyKey& Key : Keys)
	{
		FOccupant& Occupant = Occupancy.Add(Key);
		Occupant.bReserved = true;
		Occupant.ReservationId = Handle.Id;
		Occupant.HolderKey = FObjectKey(Owner);
		Occupant.Holder = Owner;
	}
	return Handle;
}

bool UBuildingSubsystem::ReleaseReservation(const FBuildingReservationHandle& Handle, const UObject* Owner)
{
	const FReservation* Reservation = Handle.IsValid() ? Reservations.Find(Handle.Id) : nullptr;
	if (!Reservation || !Owner || Reservation->OwnerKey != FObjectKey(Owner))
	{
		return false;
	}
	for (const FBuildingOccupancyKey& Key : Reservation->Keys)
	{
		const FOccupant* Occupant = Occupancy.Find(Key);
		if (Occupant && Occupant->bReserved && Occupant->ReservationId == Handle.Id)
		{
			Occupancy.Remove(Key);
		}
	}
	Reservations.Remove(Handle.Id);
	return true;
}

bool UBuildingSubsystem::CommitReservation(const FBuildingReservationHandle& Handle, const UObject* Owner, UBuildingPieceComponent* Piece)
{
	const FReservation* Reservation = Handle.IsValid() ? Reservations.Find(Handle.Id) : nullptr;
	if (!Reservation || !Owner || Reservation->OwnerKey != FObjectKey(Owner) || !Reservation->Owner.IsValid()
		|| !IsValid(Piece) || IsPieceRegistered(Piece) || !SameKeySet(Piece->GetOccupancyKeys(), Reservation->Keys))
	{
		return false;
	}
	for (const FBuildingOccupancyKey& Key : Reservation->Keys)
	{
		const FOccupant* Occupant = Occupancy.Find(Key);
		if (!Occupant || !Occupant->bReserved || Occupant->ReservationId != Handle.Id)
		{
			return false;
		}
	}
	const FObjectKey PieceKey(Piece);
	for (const FBuildingOccupancyKey& Key : Reservation->Keys)
	{
		FOccupant& Occupant = Occupancy.FindChecked(Key);
		Occupant.bReserved = false;
		Occupant.ReservationId = 0;
		Occupant.HolderKey = PieceKey;
		Occupant.Holder = Piece;
	}
	FPieceRecord& Record = Pieces.Add(PieceKey);
	Record.Piece = Piece;
	Record.Keys = Reservation->Keys;
	Reservations.Remove(Handle.Id);
	return true;
}

bool UBuildingSubsystem::RegisterActivePiece(UBuildingPieceComponent* Piece)
{
	if (!IsValid(Piece))
	{
		return false;
	}
	const TArray<FBuildingOccupancyKey> Keys = Piece->GetOccupancyKeys();
	if (Keys.IsEmpty() || HasDuplicateKeys(Keys))
	{
		return false;
	}
	if (const FPieceRecord* Existing = Pieces.Find(FObjectKey(Piece)))
	{
		// Idempotent for the same footprint; a piece never claims a second footprint.
		return SameKeySet(Existing->Keys, Keys);
	}
	PurgeStaleEntries();
	for (const FBuildingOccupancyKey& Key : Keys)
	{
		if (!IsKeyFree(Key))
		{
			return false;
		}
	}
	const FObjectKey PieceKey(Piece);
	for (const FBuildingOccupancyKey& Key : Keys)
	{
		FOccupant& Occupant = Occupancy.Add(Key);
		Occupant.bReserved = false;
		Occupant.ReservationId = 0;
		Occupant.HolderKey = PieceKey;
		Occupant.Holder = Piece;
	}
	FPieceRecord& Record = Pieces.Add(PieceKey);
	Record.Piece = Piece;
	Record.Keys = Keys;
	return true;
}

void UBuildingSubsystem::UnregisterPiece(const UBuildingPieceComponent* Piece)
{
	if (!Piece)
	{
		return;
	}
	// FObjectKey still identifies a piece that is being destroyed or already garbage.
	const FObjectKey PieceKey(Piece);
	const FPieceRecord* Record = Pieces.Find(PieceKey);
	if (!Record)
	{
		return;
	}
	for (const FBuildingOccupancyKey& Key : Record->Keys)
	{
		const FOccupant* Occupant = Occupancy.Find(Key);
		if (Occupant && !Occupant->bReserved && Occupant->HolderKey == PieceKey)
		{
			Occupancy.Remove(Key);
		}
	}
	Pieces.Remove(PieceKey);
}

int32 UBuildingSubsystem::PurgeStaleEntries()
{
	int32 Freed = 0;
	for (auto It = Reservations.CreateIterator(); It; ++It)
	{
		if (!It.Value().Owner.IsValid())
		{
			for (const FBuildingOccupancyKey& Key : It.Value().Keys)
			{
				const FOccupant* Occupant = Occupancy.Find(Key);
				if (Occupant && Occupant->bReserved && Occupant->ReservationId == It.Key())
				{
					Occupancy.Remove(Key);
					++Freed;
				}
			}
			It.RemoveCurrent();
		}
	}
	for (auto It = Pieces.CreateIterator(); It; ++It)
	{
		if (!It.Value().Piece.IsValid())
		{
			for (const FBuildingOccupancyKey& Key : It.Value().Keys)
			{
				const FOccupant* Occupant = Occupancy.Find(Key);
				if (Occupant && !Occupant->bReserved && Occupant->HolderKey == It.Key())
				{
					Occupancy.Remove(Key);
					++Freed;
				}
			}
			It.RemoveCurrent();
		}
	}
	// Orphaned entries (no reservation or piece record) cannot normally exist; drop them if they do.
	for (auto It = Occupancy.CreateIterator(); It; ++It)
	{
		if (!IsOccupantAlive(It.Value()))
		{
			It.RemoveCurrent();
			++Freed;
		}
	}
	return Freed;
}

TArray<FString> UBuildingSubsystem::AuditRegistry() const
{
	TArray<FString> Issues;
	for (const TPair<FBuildingOccupancyKey, FOccupant>& Entry : Occupancy)
	{
		const FString Key = Entry.Key.ToString();
		if (Entry.Value.bReserved)
		{
			const FReservation* Reservation = Reservations.Find(Entry.Value.ReservationId);
			if (!Reservation)
			{
				Issues.Add(FString::Printf(TEXT("%s is reserved by unknown reservation %lld."), *Key, Entry.Value.ReservationId));
			}
			else if (!Reservation->Keys.Contains(Entry.Key))
			{
				Issues.Add(FString::Printf(TEXT("%s is not listed in reservation %lld."), *Key, Entry.Value.ReservationId));
			}
			else if (!Reservation->Owner.IsValid())
			{
				Issues.Add(FString::Printf(TEXT("%s is held by reservation %lld whose owner is gone."), *Key, Entry.Value.ReservationId));
			}
			if (Entry.Value.ReservationId <= 0 || Entry.Value.ReservationId >= NextReservationId)
			{
				Issues.Add(FString::Printf(TEXT("%s has an impossible reservation id %lld."), *Key, Entry.Value.ReservationId));
			}
			continue;
		}
		const FPieceRecord* Record = Pieces.Find(Entry.Value.HolderKey);
		if (!Record)
		{
			Issues.Add(FString::Printf(TEXT("%s is occupied by an unregistered piece."), *Key));
		}
		else if (!Record->Keys.Contains(Entry.Key))
		{
			Issues.Add(FString::Printf(TEXT("%s is not listed by its occupying piece."), *Key));
		}
		else if (!Record->Piece.IsValid())
		{
			Issues.Add(FString::Printf(TEXT("%s is occupied by a destroyed piece."), *Key));
		}
	}
	for (const TPair<int64, FReservation>& Entry : Reservations)
	{
		for (const FBuildingOccupancyKey& Key : Entry.Value.Keys)
		{
			const FOccupant* Occupant = Occupancy.Find(Key);
			if (!Occupant || !Occupant->bReserved || Occupant->ReservationId != Entry.Key)
			{
				Issues.Add(FString::Printf(TEXT("Reservation %lld lost its key %s."), Entry.Key, *Key.ToString()));
			}
		}
	}
	for (const TPair<FObjectKey, FPieceRecord>& Entry : Pieces)
	{
		const UBuildingPieceComponent* Piece = Entry.Value.Piece.Get();
		for (const FBuildingOccupancyKey& Key : Entry.Value.Keys)
		{
			const FOccupant* Occupant = Occupancy.Find(Key);
			if (!Occupant || Occupant->bReserved || Occupant->HolderKey != Entry.Key)
			{
				Issues.Add(FString::Printf(TEXT("Piece lost its key %s."), *Key.ToString()));
			}
		}
		if (!Piece)
		{
			Issues.Add(TEXT("A registered piece is destroyed (stale record)."));
		}
		else if (!SameKeySet(Piece->GetOccupancyKeys(), Entry.Value.Keys))
		{
			Issues.Add(FString::Printf(TEXT("Piece %s claims keys that differ from its registration."), *Piece->GetPathName()));
		}
	}
	return Issues;
}

void UBuildingSubsystem::Deinitialize()
{
	Occupancy.Reset();
	Reservations.Reset();
	Pieces.Reset();
	Super::Deinitialize();
}

// ---- Placement validation ----

FBuildingPlacementResult UBuildingSubsystem::ValidatePlacement(TSubclassOf<UBuildingDefinition> DefinitionClass,
	const FBuildingPlacement& Requested, const AActor* Builder, const FBuildingReservationHandle& IgnoreReservation,
	const AActor* IgnoreActor) const
{
	FBuildingPlacementResult Result;
	Result.Placement = Requested;

	FString Diagnostic;
	const UBuildingDefinition* Definition = UBuildingDefinition::ResolveValidBuilding(DefinitionClass, Diagnostic);
	if (!Definition)
	{
		return MakeFailure(Result, EBuildResult::InvalidDefinition, Diagnostic);
	}

	// Placement shape.
	const bool bEdge = Definition->PlacementMode == EBuildingPlacementMode::GridEdge;
	if (Requested.PlacementMode != Definition->PlacementMode)
	{
		return MakeFailure(Result, EBuildResult::InvalidPlacement, TEXT("The placement mode does not match the building."));
	}
	if (bEdge != (Requested.EdgeAxis != EBuildingEdgeAxis::None))
	{
		return MakeFailure(Result, EBuildResult::InvalidPlacement,
			bEdge ? TEXT("An edge placement needs an X or Y edge axis.") : TEXT("A cell placement must not have an edge axis."));
	}
	if (Requested.Rotation < 0 || Requested.Rotation > 3)
	{
		return MakeFailure(Result, EBuildResult::InvalidPlacement, FString::Printf(TEXT("Rotation %d is outside 0-3."), Requested.Rotation));
	}
	UWorld* World = GetWorld();
	if (!IsValid(Builder) || !World)
	{
		return MakeFailure(Result, EBuildResult::InvalidPlacement, TEXT("A builder in this world is required."));
	}

	// Build range, measured horizontally to the piece center.
	FBuildingPlacement Placement = Requested;
	Placement.WorldTransform = ComputeWorldTransform(*Definition, Placement);
	const double Range = Definition->BuildRangeOverride > 0.f ? Definition->BuildRangeOverride : DefaultBuildRange;
	if (FVector::DistSquared2D(Builder->GetActorLocation(), Placement.WorldTransform.GetLocation()) > FMath::Square(Range))
	{
		Result.Placement = Placement;
		return MakeFailure(Result, EBuildResult::OutOfRange, FString::Printf(TEXT("The piece is farther than %.0f cm."), Range));
	}

	// Support surface: every sample needs a solid surface; WorldStatic geometry and Active floors support,
	// any other solid actor at a sample is a blocker. Category priority: NoGround, Blocked, TooSteep, Uneven.
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(BuildingSurfaceTrace), false);
	TraceParams.AddIgnoredActor(Builder);
	if (IgnoreActor)
	{
		TraceParams.AddIgnoredActor(IgnoreActor);
	}
	const double MinNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxSlopeDegrees));
	double MinZ = TNumericLimits<double>::Max();
	double MaxZ = TNumericLimits<double>::Lowest();
	AActor* SupportBlocker = nullptr;
	bool bSteep = false;
	// Edge pieces: whether every sample on the negative / positive side of the edge line is an Active floor.
	const double EdgeLine = Placement.EdgeAxis == EBuildingEdgeAxis::Y ? Placement.Anchor.X * CellSize : Placement.Anchor.Y * CellSize;
	bool bFloorSide[2] = { true, true };
	for (const FVector2D& Sample : GetSurfaceSamplePoints(*Definition, Placement))
	{
		FHitResult Hit;
		const FVector Start(Sample.X, Sample.Y, Requested.SurfaceZ + TraceHalfHeight);
		const FVector End(Sample.X, Sample.Y, Requested.SurfaceZ - TraceHalfHeight);
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, TraceParams))
		{
			Result.Placement = Placement;
			return MakeFailure(Result, EBuildResult::NoGround,
				FString::Printf(TEXT("No ground under (%.0f, %.0f)."), Sample.X, Sample.Y));
		}
		if (!IsSupportSurface(Hit) && !SupportBlocker)
		{
			SupportBlocker = Hit.GetActor();
		}
		if (bEdge)
		{
			const double Across = Placement.EdgeAxis == EBuildingEdgeAxis::Y ? Sample.X : Sample.Y;
			bFloorSide[Across < EdgeLine ? 0 : 1] &= IsActiveFloorHit(Hit);
		}
		bSteep |= Hit.ImpactNormal.Z < MinNormalZ;
		MinZ = FMath::Min(MinZ, Hit.ImpactPoint.Z);
		MaxZ = FMath::Max(MaxZ, Hit.ImpactPoint.Z);
	}
	Placement.SurfaceZ = MaxZ;
	Placement.Layer = HeightToLayer(MaxZ);
	Placement.WorldTransform = ComputeWorldTransform(*Definition, Placement);
	Result.Placement = Placement;
	Result.Keys = ComputeOccupancyKeys(*Definition, Placement);
	if (SupportBlocker)
	{
		return MakeFailure(Result, EBuildResult::Blocked,
			FString::Printf(TEXT("%s is in the way."), *SupportBlocker->GetName()), SupportBlocker);
	}
	if (bSteep)
	{
		return MakeFailure(Result, EBuildResult::TooSteep, FString::Printf(TEXT("The surface is steeper than %.0f degrees."), MaxSlopeDegrees));
	}
	if (MaxZ - MinZ > MaxUnevenness)
	{
		return MakeFailure(Result, EBuildResult::Uneven, FString::Printf(TEXT("The surface varies by %.0f cm."), MaxZ - MinZ));
	}
	if (bEdge && Definition->bRequiresFloor && !bFloorSide[0] && !bFloorSide[1])
	{
		return MakeFailure(Result, EBuildResult::NeedsFloor, TEXT("Needs an Active floor along one side of the edge."));
	}

	// Occupancy.
	for (const FBuildingOccupancyKey& Key : Result.Keys)
	{
		if (!IsKeyFree(Key, IgnoreReservation))
		{
			return MakeFailure(Result, EBuildResult::Occupied, FString::Printf(TEXT("%s is already taken."), *Key.ToString()));
		}
	}

	// World and pawn collision: anything solid to pawns inside the piece volume, except the builder (checked next).
	const FBox Box = GetCollisionBox(*Definition, Placement);
	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(BuildingPlacementOverlap), false);
	OverlapParams.AddIgnoredActor(Builder);
	if (IgnoreActor)
	{
		OverlapParams.AddIgnoredActor(IgnoreActor);
	}
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Box.GetCenter(), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeBox(Box.GetExtent()), OverlapParams);
	TArray<AActor*> Blockers;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Overlap.bBlockingHit && Actor)
		{
			Blockers.AddUnique(Actor);
		}
	}
	if (!Blockers.IsEmpty())
	{
		// Deterministic choice of the reported blocker.
		Blockers.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
		return MakeFailure(Result, EBuildResult::Blocked, FString::Printf(TEXT("%s is in the way."), *Blockers[0]->GetName()), Blockers[0]);
	}

	// Builder safety: the piece may not touch the builder's collision, with a small clearance.
	if (const UPrimitiveComponent* BuilderRoot = Cast<UPrimitiveComponent>(Builder->GetRootComponent()))
	{
		const FCollisionShape Inflated = FCollisionShape::MakeBox(Box.GetExtent() + FVector(BuilderClearance));
		if (BuilderRoot->IsCollisionEnabled() && BuilderRoot->OverlapComponent(Box.GetCenter(), FQuat::Identity, Inflated))
		{
			return MakeFailure(Result, EBuildResult::BlocksBuilder, TEXT("The piece would trap the builder."),
				const_cast<AActor*>(Builder));
		}
	}

	Result.Result = EBuildResult::Success;
	Result.Diagnostic.Reset();
	return Result;
}
