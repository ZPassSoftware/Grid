#include "Components/GridNavigationComponent.h"
#include "LogGridRuntime.h"
#include "Grids/Grid.h"
#include "Subsystems/GridSubsystem.h"
#include "GridAgents/DefaultGridNavigationAgent.h"
#include "Util/GridUtilities.h"

UGridNavigationComponent::UGridNavigationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	OwnerPawn = nullptr;
	OwnerController = nullptr;

	FollowingPathIndex = 0;
	bIsMoving = false;

	NavMode = EGridNavMode::GridBased;

	AgentClasses.Add(UDefaultGridNavigationAgent::StaticClass());
}

UGridNavigationComponent::~UGridNavigationComponent()
{

}

void UGridNavigationComponent::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn)
	{
		OwnerController = Cast<AAIController>(OwnerPawn->GetController());
		if(!OwnerController)
		{
			FLogGridRuntime::Warning("UGridNavigationComponent Cast<AAIController> failed, OwnerController is not AAIController");
		}
	}
	else
	{
		FLogGridRuntime::Warning("UGridNavigationComponent Cast<APawn> failed, Owner is not APawn");
	}

	for(auto AgentClass : AgentClasses)
	{
		if (UGridNavigationAgent* Agent = NewObject<UGridNavigationAgent>(this, AgentClass))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, "OnMoveCompleted");

			Agent->OnMoveCompleted.Add(Delegate);
			Agents.Add(Agent);
			Agent->BeginPlay();
		}
		else
		{
			FLogGridRuntime::Error("UGridNavigationComponent::BeginPlay create grid navigation agent failed!");
		}
	}
}

UGrid* UGridNavigationComponent::GetOwnerGridPosition()
{
	if (!OwnerPawn)
	{
		FLogGridRuntime::Error("UGridNavigationComponent::GetOwnerGridPosition failed, OwnerPawn is null");
		return nullptr;
	}

	UGridSubsystem* GridSubsystem = GetGridSubsystem();
	if (!ensure(GridSubsystem != nullptr))
	{
		FLogGridRuntime::Error("UGridNavigationComponent::GetOwnerGridPosition failed, GridSubsystem is null");
		return nullptr;
	}
	
	return GridSubsystem->GetGridByPosition(OwnerPawn->GetActorLocation());

}

void UGridNavigationComponent::GetReachableGridsByRange(const int32 Range, TArray<UGrid*>& Grids)
{

	if (!OwnerPawn)
	{
		FLogGridRuntime::Error("UGridNavigationComponent::GetReachableGridsByRange failed, OwnerPawn is null");
		return;
	}
	
	UGridSubsystem* GridSubsystem = GetGridSubsystem();
	
	if (!ensure(GridSubsystem != nullptr))
	{
		FLogGridRuntime::Error("UGridNavigationComponent::GetReachableGridsByRange failed, GridSubsystem is null");
		return;
	}
	
	GridSubsystem->GetPathFinder()->GetReachableGrids(OwnerPawn, Range, Grids);

	Grids.Remove(GetOwnerGridPosition());
}

bool UGridNavigationComponent::RequestMove(UGrid* DestGrid)
{
	if (!OwnerPawn)
	{
		FLogGridRuntime::Error("UGridNavigationComponent::RequestMove failed, OwnerPawn is null");
		return false;
	}

	if (!OwnerController)
	{
		FLogGridRuntime::Error("UGridNavigationComponent::RequestMove failed, OwnerController is null");
		return false;
	}

	if (!DestGrid)
	{
		FLogGridRuntime::Warning("UGridNavigationComponent::RequestMove failed, DestGrid is null");
		return false;
	}

	UGridSubsystem* GridSubsystem = DestGrid->GridSubsystem;

	if (!ensure(GridSubsystem != nullptr))
	{
		FLogGridRuntime::Error("UGridNavigationComponent::RequestMove failed, GridSubsystem is null");
		return false;
	}

	FGridPathfindingRequest Request;
	TArray<UGrid*> Result;

	Request.Sender = OwnerPawn;
	Request.Destination = DestGrid;
	Request.Start = GetOwnerGridPosition();

	UGridPathfinder* PathFinder = GridSubsystem->GetPathFinder();
	
	if (!PathFinder)
	{
		FLogGridRuntime::Error("UGridNavigationComponent::RequestMove failed, PathFinder is null");
		return false;
	}
	
	PathFinder->Reset();
	
	if (!PathFinder->FindPath(Request,CurrentFollowingPath))
	{
		FLogGridRuntime::Error("UGridNavigationComponent::RequestMove failed, UGridUtilities::FindPath is false, check navigation mesh");
		return false;
	}

	FollowingPathIndex = 0;

	bIsMoving = true;

	MoveToNext();

	return true;
}

void UGridNavigationComponent::StopMove()
{
	if (CurrentAgent)
	{
		CurrentAgent->StopMove();
		CurrentAgent = nullptr;
	}
}

bool UGridNavigationComponent::IsMoving() const
{
	return bIsMoving;
}

bool UGridNavigationComponent::MoveToNext()
{
	switch (NavMode)
	{
	case EGridNavMode::GridBased:
		return MoveToNextGrid();
	case EGridNavMode::Free:
		return MoveToNextPoint();
	default:
		return false;
	}
}

bool UGridNavigationComponent::MoveToNextGrid()
{
	++FollowingPathIndex;

	if (FollowingPathIndex >= CurrentFollowingPath.Num())
	{
		return false;
	}

	UGridSubsystem* GridSubsystem = CurrentFollowingPath.Last()->GridSubsystem;

	UGrid* CurrGrid = GridSubsystem->GetGridByPosition(OwnerPawn->GetActorLocation());
	UGrid* NextGrid = CurrentFollowingPath[FollowingPathIndex];

	CurrentAgent = FindAgent(CurrGrid, NextGrid);

	if (!CurrentAgent)
	{
		FLogGridRuntime::Error("UGridNavigationComponent::MoveToNextGrid can't find proper agent");
		return false;
	}

	CurrentAgent->RequestMove(OwnerPawn, CurrGrid, NextGrid);

	return true;
}

bool UGridNavigationComponent::MoveToNextPoint()
{
	++FollowingPathIndex;

	if (FollowingPathIndex >= CurrentFollowingPath.Num())
	{
		return false;
	}
	
	UGridSubsystem* GridSubsystem = CurrentFollowingPath.Last()->GridSubsystem;

	UGrid* CurrGrid = GridSubsystem->GetGridByPosition(OwnerPawn->GetActorLocation());
	UGrid* NextGrid = CurrentFollowingPath[FollowingPathIndex];

	UGridNavigationAgent* Agent = FindAgent(CurrGrid, NextGrid);
	
	if (!Agent)
	{
		FLogGridRuntime::Error("UGridNavigationComponent::MoveToNextGrid can't find proper agent");
		return false;
	}

	if (Cast<UDefaultGridNavigationAgent>(Agent))
	{
		int i;
		for (i = FollowingPathIndex; i < CurrentFollowingPath.Num() - 1; ++i)
		{
			if (!Agent->Check(OwnerPawn, CurrentFollowingPath[i], CurrentFollowingPath[i + 1]))
			{
				break;
			}
		}

		FollowingPathIndex = i;
		NextGrid = CurrentFollowingPath[FollowingPathIndex];
	}

	Agent->RequestMove(OwnerPawn, CurrGrid, NextGrid);

	return true;
}

UGridNavigationAgent* UGridNavigationComponent::FindAgent(UGrid* Start, UGrid* Goal)
{
	for (auto Agent : Agents)
	{
		if (Agent->Check(OwnerPawn, Start, Goal))
		{
			return Agent;
		}
	}
	FLogGridRuntime::Warning("UGridNavigationComponent::FindAgent Agent not found");
	return nullptr;
}

void UGridNavigationComponent::OnMoveCompleted(APawn* Pawn, bool bSuccess)
{
	if (bSuccess)
	{
		if (FollowingPathIndex < CurrentFollowingPath.Num() - 1)
		{
			if (NavMode == EGridNavMode::GridBased)
				OnArrivalNewGrid.Broadcast(this);
		}
		else
		{
			OnArrivalGoal.Broadcast(this);
		}

		if (!MoveToNext())
		{
			bIsMoving = false;
		}
	}
	else
	{
		FLogGridRuntime::Error("UGridNavigationComponent::OnMoveCompleted failed");

		bIsMoving = false;
	}
}

UGridSubsystem* UGridNavigationComponent::GetGridSubsystem() const
{
	for (auto GridSubsystem : GetWorld()->GetSubsystemArray<UGridSubsystem>())
	{
		if (GridSubsystem && GridSubsystem->IsInitialized())
		{
			return GridSubsystem;
		}
	}
	FLogGridRuntime::Warning("UGridSensingComponent::GetGridSubsystem GridSubsystem not found");
	return nullptr;
}
