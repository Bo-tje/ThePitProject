#include "HarmonicCrystalActor.h"
#include "Components/SceneComponent.h"

AHarmonicCrystalActor::AHarmonicCrystalActor()
{
	PrimaryActorTick.bCanEverTick = true;

	CrystalRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CrystalRoot"));
	SetRootComponent(CrystalRoot);
}

void AHarmonicCrystalActor::SetupCrystal(int32 InStationIndex, FLinearColor InColor)
{
	StationIndex = InStationIndex;
	CrystalColor = InColor;
	RemainingLifespan = ActiveLifespanSeconds;
	OnCrystalSetup(StationIndex, CrystalColor);
}

void AHarmonicCrystalActor::SetCharging(bool bNewCharging)
{
	bIsCharging = bNewCharging;
	if (bIsCharging)
	{
		bLifespanDecaying = false;
	}
}

void AHarmonicCrystalActor::TriggerBeatPulse(float Intensity)
{
	PulseFlash = Intensity;
	OnCrystalBeatPulse(Intensity, CrystalColor);
}

void AHarmonicCrystalActor::ResetCrystal()
{
	bIsCharging = false;
	bIsActive = false;
	bLifespanDecaying = false;
	ChargeProgress = 0.0f;
	PulseFlash = 0.0f;
	RemainingLifespan = ActiveLifespanSeconds;
	OnCrystalCharge(0.0f, CrystalColor);
	OnCrystalDeactivated();
}

void AHarmonicCrystalActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PulseFlash > 0.001f)
	{
		PulseFlash = FMath::FInterpTo(PulseFlash, 0.0f, DeltaTime, 10.0f);
	}

	if (bIsCharging)
	{
		const float EffectiveChargeRate = (1.0f / FMath::Max(0.1f, ChargeThresholdSeconds)) * ChargeRate;
		ChargeProgress = FMath::Clamp(ChargeProgress + DeltaTime * EffectiveChargeRate, 0.0f, 1.0f);
		OnCrystalCharge(ChargeProgress, CrystalColor);

		if (ChargeProgress >= 1.0f)
		{
			if (bIsActive)
			{
				RemainingLifespan = ActiveLifespanSeconds;
			}
			else if (bCanActivate)
			{
				bIsActive = true;
				RemainingLifespan = ActiveLifespanSeconds;
				PulseFlash = 1.0f;
				OnCrystalActivated(CrystalColor);
				OnCrystalActivatedEvent.Broadcast(this);
			}
		}
	}
	else if (bIsActive)
	{
		RemainingLifespan -= DeltaTime;
		if (RemainingLifespan <= 0.0f)
		{
			bIsActive = false;
			RemainingLifespan = 0.0f;
			bLifespanDecaying = true;
			OnCrystalExpiredEvent.Broadcast(this);
		}
	}
	else if (ChargeProgress > 0.0f)
	{
		ChargeProgress = FMath::FInterpTo(ChargeProgress, 0.0f, DeltaTime, ChargeDecayRate);
		if (ChargeProgress <= 0.001f)
		{
			ChargeProgress = 0.0f;
		}

		OnCrystalCharge(ChargeProgress, CrystalColor);

		if (ChargeProgress <= 0.0f && bLifespanDecaying)
		{
			bLifespanDecaying = false;
			OnCrystalDeactivated();
		}
	}
}
