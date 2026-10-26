// Fill out your copyright notice in the Description page of Project Settings.

#include "PressureGenerator.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

APressureGenerator::APressureGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootSceneComponent;

	CoreMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMeshComponent->SetupAttachment(RootComponent);
}

void APressureGenerator::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	if (bAutoStartSpawning)
	{
		StartSpawning();
	}
}

void APressureGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();
	Super::EndPlay(EndPlayReason);
}

void APressureGenerator::StartSpawning()
{
	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &APressureGenerator::SpawnRandomNote, SpawnInterval, true);
}

void APressureGenerator::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void APressureGenerator::SpawnRandomNote()
{
	if (SplineTracks.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, SplineTracks.Num() - 1);
		SpawnNoteOnTrack(RandomIndex);
	}
}

void APressureGenerator::SpawnNoteOnTrack(int32 TrackIndex)
{
	if (!SplineTracks.IsValidIndex(TrackIndex) || !NoteClass || !GetWorld())
	{
		return;
	}

	USplineComponent* Track = SplineTracks[TrackIndex];
	if (!Track)
	{
		return;
	}

	FVector SpawnLoc = Track->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World);
	FRotator SpawnRot = Track->GetRotationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	GetWorld()->SpawnActor<AActor>(NoteClass, SpawnLoc, SpawnRot, SpawnParams);
}

void APressureGenerator::ApplyGeneratorDamage(float DamageAmount)
{
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	OnPressureCoreHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		StopSpawning();
		OnPressureCoreOverload.Broadcast();
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


