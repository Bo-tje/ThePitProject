// Fill out your copyright notice in the Description page of Project Settings.


#include "PressureGenerator.h"

#include "Components/SplineComponent.h"

APressureGenerator::APressureGenerator()
{
	PrimaryActorTick.bCanEverTick = false; // Generator doesn't need to tick if driven by timers!
    
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootSceneComponent;
    
	CoreMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMeshComponent->SetupAttachment(RootComponent);
}

void APressureGenerator::StartSpawning()
{
	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &APressureGenerator::SpawnRandomNote, SpawnInterval, true);
}

void APressureGenerator::SpawnRandomNote()
{
	if (SplineTracks.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, SplineTracks.Num() - 1);
		
		SpawnNoteOnTrack(RandomIndex);
	}

}

USplineComponent* APressureGenerator::CreateNewTrack()
{
	USplineComponent* NewTrack = NewObject<USplineComponent>(this);
	if (NewTrack)
	{
		NewTrack->RegisterComponent();
		NewTrack->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		SplineTracks.Add(NewTrack);
	}
	return NewTrack;
}

