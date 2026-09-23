    #pragma once
    
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PressureGenerator.generated.h"
    
class USplineComponent;
class UStaticMeshComponent;
    
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPressureCoreHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPressureCoreOverload);

UCLASS()
class THEPITPROJECT_API APressureGenerator : public AActor
{
    GENERATED_BODY()
    
public:
    APressureGenerator();
    
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> RootSceneComponent;
        
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> CoreMeshComponent;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<TObjectPtr<USplineComponent>> SplineTracks;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure|Spawning")
    TSubclassOf<AActor> NoteClass;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure|Spawning")
    float NoteTravelSpeed = 600.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure|Spawning")
    float SpawnInterval = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure|Spawning")
    bool bAutoStartSpawning = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure|Health")
    float MaxHealth = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure|Spawning")
    float CurrentHealth = 100.0f;
    
    FTimerHandle SpawnTimerHandle;

public:
    UFUNCTION(BlueprintCallable, Category = "Pressure|Controllable")
    void StartSpawning();
    
    UFUNCTION(BlueprintCallable, Category = "Pressure|Controllable")
    void StopSpawning();
    
    UFUNCTION(BlueprintCallable, Category = "Pressure|Controllable")
    void SpawnNoteOnTrack(int32 TrackIndex);
    
    UFUNCTION(BlueprintCallable, Category = "Pressure|Controllable")
    void SpawnRandomNote();
    
    UFUNCTION(BlueprintCallable, Category = "Pressure|Controllable")
    void ApplyGeneratorDamage(float DamageAmount);
    
    UFUNCTION(BlueprintCallable, Category = "Pressure|Controllable")
    USplineComponent* CreateNewTrack();
    
    UPROPERTY(BlueprintAssignable, Category = "Pressure|Events")
    FOnPressureCoreHealthChanged OnPressureCoreHealthChanged;
    
    UPROPERTY(BlueprintAssignable, Category = "Pressure|Events")
    FOnPressureCoreOverload OnPressureCoreOverload;
};