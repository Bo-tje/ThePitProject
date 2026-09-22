// Fill out your copyright notice in the Description page of Project Settings.

#include "PitShootyPlayer.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

APitShootyPlayer::APitShootyPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(FVector(50.0f, 40.0f, 20.0f));
	CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
	SetRootComponent(CollisionBox);

	CarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarMesh"));
	CarMesh->SetupAttachment(RootComponent);

	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	ShieldMesh->SetupAttachment(RootComponent);
	ShieldMesh->SetVisibility(false);
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	MuzzleLocation->SetupAttachment(RootComponent);
	MuzzleLocation->SetRelativeLocation(FVector(60.0f, 0.0f, 0.0f));

	CurrentHealth = MaxHealth;
	CurrentShieldEnergy = MaxShieldEnergy;
}

void APitShootyPlayer::BeginPlay()
{
	Super::BeginPlay();

	InitialSpawnLocation = GetActorLocation();
	CurrentHealth = MaxHealth;
	CurrentShieldEnergy = MaxShieldEnergy;

	// Automatically find Track Actor if configured
	if (!TrackActor && bFindTrackActorOnBeginPlay)
	{
		if (TrackActorClass)
		{
			TrackActor = UGameplayStatics::GetActorOfClass(GetWorld(), TrackActorClass);
		}
	}

	if (TrackActor)
	{
		SetTrackActor(TrackActor);
		UpdateTransformOnSpline();
	}
}

void APitShootyPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle shield energy recharge / drain
	if (bIsShieldActive)
	{
		CurrentShieldEnergy = FMath::Max(0.0f, CurrentShieldEnergy - ShieldDrainRate * DeltaTime);
		if (CurrentShieldEnergy <= 0.0f)
		{
			SetShieldActive(false);
		}
	}
	else
	{
		CurrentShieldEnergy = FMath::Min(MaxShieldEnergy, CurrentShieldEnergy + ShieldRechargeRate * DeltaTime);
	}
}

void APitShootyPlayer::SetTrackActor(AActor* NewTrackActor)
{
	TrackActor = NewTrackActor;
	if (TrackActor)
	{
		TargetSplineComponent = TrackActor->FindComponentByClass<USplineComponent>();
	}
	else
	{
		TargetSplineComponent = nullptr;
	}
}

void APitShootyPlayer::OnPlayerAssigned_Implementation(int32 PlayerIndex, FName Channel)
{
	AssignedPlayerIndex = PlayerIndex;
	AssignedChannel = Channel;
	UE_LOG(LogTemp, Log, TEXT("APitShootyPlayer assigned to Player %d with channel %s"), PlayerIndex, *Channel.ToString());
}

void APitShootyPlayer::OnActionPressed_Implementation(FName Channel)
{
	FString ChannelStr = Channel.ToString().ToLower();

	// Ignore axis channels
	if (ChannelStr.Contains(TEXT("axis")) || 
	    ChannelStr.Contains(TEXT("dial")) || 
	    ChannelStr.Contains(TEXT("rotary")) || 
	    ChannelStr.Contains(TEXT("steer")) ||
	    ChannelStr.Contains(TEXT("wheel")) ||
	    ChannelStr == TEXT("x"))
	{
		return;
	}

	if (ChannelStr.Contains(TEXT("shield")) || ChannelStr.Contains(TEXT("defend")))
	{
		SetShieldActive(true);
	}
	else
	{
		// Default or "action", "fire", "button"
		Fire();
	}
}

void APitShootyPlayer::OnActionReleased_Implementation(FName Channel)
{
	FString ChannelStr = Channel.ToString().ToLower();

	if (ChannelStr.Contains(TEXT("shield")) || ChannelStr.Contains(TEXT("defend")))
	{
		SetShieldActive(false);
	}
}

void APitShootyPlayer::OnAxisInput_Implementation(FName Channel, float Value, float Delta)
{
	FString ChannelStr = Channel.ToString().ToLower();

	// Ignore button and action subchannels for movement
	if (ChannelStr.Contains(TEXT("action")) || 
	    ChannelStr.Contains(TEXT("button")) || 
	    ChannelStr.Contains(TEXT("shield")) || 
	    ChannelStr.Contains(TEXT("defend")) || 
	    ChannelStr.Contains(TEXT("fire")) ||
	    ChannelStr.Contains(TEXT("trigger")))
	{
		return;
	}

	// If joystick Y axis is sent as "y", "vertical", or "down"
	if (ChannelStr.Contains(TEXT("y")) || ChannelStr.Contains(TEXT("vert")))
	{
		if (Value < -0.4f)
		{
			SetShieldActive(true);
		}
		else if (Value > 0.4f)
		{
			SetShieldActive(false);
			Fire();
		}
		else
		{
			SetShieldActive(false);
		}
		return;
	}

	// Horizontal axis movement (axis, dial, x, steer, move, or default)
	float MoveAmount = (FMath::Abs(Delta) > KINDA_SMALL_NUMBER) ? Delta : Value;

	if (TargetSplineComponent || TrackActor)
	{
		MoveAlongSpline(MoveAmount);
	}
	else
	{
		MoveAlongRail(MoveAmount);
	}
}

void APitShootyPlayer::MoveAlongSpline(float InputDelta)
{
	if (!TargetSplineComponent)
	{
		if (TrackActor)
		{
			TargetSplineComponent = TrackActor->FindComponentByClass<USplineComponent>();
		}
	}

	if (!TargetSplineComponent)
	{
		return;
	}

	float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	float SplineLength = TargetSplineComponent->GetSplineLength();

	CurrentDistance = FMath::Clamp(CurrentDistance + (InputDelta * MovementSpeed * DeltaSeconds), 0.0f, SplineLength);

	UpdateTransformOnSpline();
}

void APitShootyPlayer::UpdateTransformOnSpline()
{
	if (!TargetSplineComponent)
	{
		return;
	}

	FVector NewLocation = TargetSplineComponent->GetLocationAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World);

	if (bUpdateRotationFromSpline)
	{
		FRotator NewRotation = TargetSplineComponent->GetRotationAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World);
		SetActorLocationAndRotation(NewLocation, NewRotation);
	}
	else
	{
		SetActorLocation(NewLocation);
	}
}

void APitShootyPlayer::MoveAlongRail(float InputDelta)
{
	if (FMath::IsNearlyZero(InputDelta))
	{
		return;
	}

	float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	CurrentPositionOnRail = FMath::Clamp(CurrentPositionOnRail + (InputDelta * MovementSpeed * DeltaSeconds), MinRailPosition, MaxRailPosition);
	
	FVector NewLocation = InitialSpawnLocation + (MovementAxis.GetSafeNormal() * CurrentPositionOnRail);
	SetActorLocation(NewLocation);
}

void APitShootyPlayer::Fire()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastFireTime < FireCooldown)
	{
		return;
	}

	LastFireTime = CurrentTime;

	if (ProjectileClass)
	{
		FTransform SpawnTransform = MuzzleLocation ? MuzzleLocation->GetComponentTransform() : GetActorTransform();
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		World->SpawnActor<AActor>(ProjectileClass, SpawnTransform, SpawnParams);
	}

	OnFired.Broadcast();
}

void APitShootyPlayer::SetShieldActive(bool bActive)
{
	if (bActive && CurrentShieldEnergy <= 0.0f)
	{
		return;
	}

	bIsShieldActive = bActive;

	if (ShieldMesh)
	{
		ShieldMesh->SetVisibility(bIsShieldActive);
		ShieldMesh->SetCollisionEnabled(bIsShieldActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	OnShieldToggled.Broadcast(bIsShieldActive);
}

void APitShootyPlayer::ApplyDamageToPlayer(float DamageAmount)
{
	if (bIsShieldActive)
	{
		// Deflected / absorbed by shield
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth);

	if (CurrentHealth <= 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("APitShootyPlayer %d destroyed!"), AssignedPlayerIndex);
	}
}
