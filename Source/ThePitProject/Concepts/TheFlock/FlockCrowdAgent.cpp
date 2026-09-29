#include "FlockCrowdAgent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"

AFlockCrowdAgent::AFlockCrowdAgent()
{
	PrimaryActorTick.bCanEverTick = true;

	// Configure character movement for snappy crowd reaction
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
		MoveComp->MaxAcceleration = 2048.0f;
		MoveComp->BrakingDecelerationWalking = 1024.0f;
		MoveComp->GroundFriction = 8.0f;
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

		// Enable RVO Crowd Avoidance so agents naturally push past each other
		MoveComp->bUseRVOAvoidance = true;
		MoveComp->AvoidanceWeight = 0.5f;
		MoveComp->SetAvoidanceGroup(1);
		MoveComp->SetGroupsToAvoid(1);
	}

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AFlockCrowdAgent::BeginPlay()
{
	Super::BeginPlay();
	PickRandomWanderPoint();
}

void AFlockCrowdAgent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateMovementLogic(DeltaTime);
}

void AFlockCrowdAgent::SetTargetStationLocation(const FVector& NewTargetLocation, bool bIsStationActive)
{
	if (bIsStationActive)
	{
		CurrentTargetLocation = NewTargetLocation;
		bHasActiveTarget = true;
		CurrentState = EFlockAgentState::RunningToStation;

		if (GetCharacterMovement())
		{
			GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
		}
	}
	else
	{
		ReturnToIdle();
	}
}

void AFlockCrowdAgent::TriggerFlashMob(const FVector& CenterLocation)
{
	CurrentTargetLocation = CenterLocation;
	bHasActiveTarget = true;
	CurrentState = EFlockAgentState::FlashMobDance;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed * 0.8f;
	}

	if (DanceMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		if (!GetMesh()->GetAnimInstance()->Montage_IsPlaying(DanceMontage))
		{
			PlayAnimMontage(DanceMontage);
		}
	}
}

void AFlockCrowdAgent::ReturnToIdle()
{
	bHasActiveTarget = false;
	CurrentState = EFlockAgentState::IdleWander;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}

	PickRandomWanderPoint();
}

void AFlockCrowdAgent::UpdateMovementLogic(float DeltaTime)
{
	const FVector ActorLoc = GetActorLocation();

	if (CurrentState == EFlockAgentState::RunningToStation || CurrentState == EFlockAgentState::FlashMobDance)
	{
		const float Dist = FVector::Dist2D(ActorLoc, CurrentTargetLocation);

		if (Dist <= ArrivalDistance)
		{
			// Arrived at the railing station!
			if (CurrentState == EFlockAgentState::RunningToStation)
			{
				CurrentState = EFlockAgentState::CheeringAtRailing;

				// Face toward the railing station
				const FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(ActorLoc, CurrentTargetLocation);
				SetActorRotation(FRotator(0.0f, LookAtRot.Yaw, 0.0f));

				// Play Cheer Montage if available
				if (CheerMontage && GetMesh() && GetMesh()->GetAnimInstance())
				{
					if (!GetMesh()->GetAnimInstance()->Montage_IsPlaying(CheerMontage))
					{
						PlayAnimMontage(CheerMontage);
					}
				}
			}
		}
		else
		{
			// Direct movement vector towards target station
			FVector Direction = (CurrentTargetLocation - ActorLoc).GetSafeNormal2D();
			AddMovementInput(Direction, 1.0f);
		}
	}
	else if (CurrentState == EFlockAgentState::IdleWander)
	{
		IdleWanderTimer -= DeltaTime;
		const float Dist = FVector::Dist2D(ActorLoc, CurrentTargetLocation);

		if (Dist <= 60.0f || IdleWanderTimer <= 0.0f)
		{
			PickRandomWanderPoint();
		}
		else
		{
			FVector Direction = (CurrentTargetLocation - ActorLoc).GetSafeNormal2D();
			AddMovementInput(Direction, 0.5f);
		}
	}
}

void AFlockCrowdAgent::PickRandomWanderPoint()
{
	const float RandomAngle = FMath::RandRange(0.0f, 2.0f * PI);
	const float RandomDist = FMath::RandRange(200.0f, 600.0f);

	CurrentTargetLocation = GetActorLocation() + FVector(FMath::Cos(RandomAngle) * RandomDist, FMath::Sin(RandomAngle) * RandomDist, 0.0f);
	IdleWanderTimer = FMath::RandRange(3.0f, 7.0f);
}
