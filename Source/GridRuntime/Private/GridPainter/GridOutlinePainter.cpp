#include "GridPainter/GridOutlinePainter.h"
#include "Subsystems/GridSubsystem.h"

UGridOutlinePainter::UGridOutlinePainter()
{
	bIsTickable = true;

	GridOutlineComp = CreateDefaultSubobject<UGridOutlineComponent>("GridOutlineComp");

	OutlineThickness = 5.f;
	ZOffset = 5.f;
}

UGridOutlinePainter::~UGridOutlinePainter()
{

}

void UGridOutlinePainter::SetGridManager(UGridSubsystem* NewGridManager)
{
	Super::SetGridManager(NewGridManager);

	if (!GridOutlineComp->IsRegistered())
	{
		GridOutlineComp->RegisterComponentWithWorld(GridManager->GetWorld());
	}
}

void UGridOutlinePainter::TickImpl_Implementation(float DeltaTime)
{
	if (GridOutlineComp)
	{
		GridOutlineComp->UpdateGridInfo();
	}
}

void UGridOutlinePainter::GetColorPriority_Implementation(TArray<FLinearColor>& Colors)
{
	Colors.Reset();
	Colors.Add(FLinearColor::White);
}

void UGridOutlinePainter::GetColors_Implementation(UGrid* Grid, TArray<FLinearColor>& Colors)
{
	Colors.Reset();
	Colors.Add(FLinearColor::White);
}
