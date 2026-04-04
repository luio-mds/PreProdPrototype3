#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BossCharacter.generated.h"

// --- Phase enum --------------------------------------------------------------
UENUM(BlueprintType)
enum class EBossPhase : uint8
{
    Phase1  UMETA(DisplayName = "Phase 1"),
    Phase2  UMETA(DisplayName = "Phase 2"),
    Phase3  UMETA(DisplayName = "Phase 3")
};

// --- Body part enum (used by hit components on head / shell / claws) ---------
UENUM(BlueprintType)
enum class EBodyPart : uint8
{
    Head      UMETA(DisplayName = "Head"),
    Shell     UMETA(DisplayName = "Shell"),
    LeftClaw  UMETA(DisplayName = "Left Claw"),
    RightClaw UMETA(DisplayName = "Right Claw")
};

// --- Delegates ---------------------------------------------------------------
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossDeathSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChangedSignature, EBossPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossStaggerSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDishDrainedSignature);

// ----------------------------------------------------------------------------

UCLASS()
class COOPSOULSLIKE_API ABossCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABossCharacter();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // -- Health ---------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Health")
    float MaxHealth = 3000.f;

    UPROPERTY(BlueprintReadOnly, Category = "Boss|Health")
    float CurrentHealth;

    // Multiplier applied to all incoming damage (2.0 when dish is drained)
    UPROPERTY(BlueprintReadOnly, Category = "Boss|Health")
    float IncomingDamageMultiplier = 1.f;

    // -- Phases ---------------------------------------------------------------
    UPROPERTY(BlueprintReadOnly, Category = "Boss|Phase")
    EBossPhase CurrentPhase = EBossPhase::Phase1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Phase")
    float Phase2Threshold = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Phase")
    float Phase3Threshold = 0.3f;

    // -- Poise / Stagger ------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Poise")
    float MaxPoise = 300.f;

    UPROPERTY(BlueprintReadOnly, Category = "Boss|Poise")
    float CurrentPoise;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Poise")
    float PoiseResetTime = 2.f;

    // -- Shell Armor ----------------------------------------------------------
    // Hits from the front arc are multiplied by this (0.5 = 50% damage)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Armor")
    float ShellArmorMultiplier = 0.5f;

    // Dot product threshold - above this the hit is considered frontal
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Armor")
    float ShellArmorArcThreshold = 0.3f;

    // -- Water Dish -----------------------------------------------------------
    UPROPERTY(BlueprintReadOnly, Category = "Boss|WaterDish")
    float WaterLevel = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|WaterDish")
    float DishDrainPerHit = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|WaterDish")
    float DishRefillTime = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|WaterDish")
    float WeakenedDuration = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|WaterDish")
    float WeakenedDamageMultiplier = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|WaterDish")
    float WeakenedSpeedMultiplier = 0.7f;

    // -- Enrage ---------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Enrage")
    float EnrageTime = 180.f;

    UPROPERTY(BlueprintReadWrite, Category = "Boss|Enrage")
    bool bIsEnraged = false;

    // -- Intro ----------------------------------------------------------------
    // AI does not activate until CompleteIntro() is called
    UPROPERTY(BlueprintReadWrite, Category = "Boss|Intro")
    bool bIntroComplete = false;

    // -- Delegates ------------------------------------------------------------
    UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
    FOnBossDeathSignature OnBossDeath;

    UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
    FOnPhaseChangedSignature OnPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
    FOnBossStaggerSignature OnBossStagger;

    UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
    FOnDishDrainedSignature OnDishDrained;

    // -- Public API -----------------------------------------------------------

    // Called by hit components (head/shell/claw box colliders) when struck
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void ReceiveBodyPartHit(EBodyPart BodyPart, AActor* DamageCauser, float RawDamage);

    // Called when the player lands a hit on the head socket
    UFUNCTION(BlueprintCallable, Category = "Boss|WaterDish")
    void DrainDish(float Amount);

    // Called from Blueprint or attack tasks to apply a poise hit
    UFUNCTION(BlueprintCallable, Category = "Boss|Poise")
    void ApplyPoiseHit(float HitImpulse, bool bBypass);

    // Called from Blueprint when the intro montage finishes
    UFUNCTION(BlueprintCallable, Category = "Boss|Intro")
    void CompleteIntro();

    UFUNCTION(BlueprintPure, Category = "Boss|Health")
    bool IsAlive() const { return CurrentHealth > 0.f; }

    // -- UE overrides ---------------------------------------------------------
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

protected:
    // -- Internal helpers -----------------------------------------------------
    void CheckPhaseTransition();
    float ApplyShellArmor(float RawDamage, const FVector& HitDirection);
    void HandleDeath();
    void ResetPoise();
    void RefillDish();
    void EndWeakened();
    void TriggerEnrage();

    // Blueprint Implementable Events - implement visuals/audio in BP_KappaBoss
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Phase")
    void OnPhaseTransitionVisuals(EBossPhase NewPhase);

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Death")
    void OnDeathVisuals();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Poise")
    void OnStaggerVisuals();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss|WaterDish")
    void OnDishEmptied();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Enrage")
    void OnEnrageVisuals();

private:
    FTimerHandle PoiseResetTimer;
    FTimerHandle DishRefillTimer;
    FTimerHandle WeakenedTimer;
    FTimerHandle EnrageTimer;

    bool bIsDead = false;
    bool bIsWeakened = false;

    // Cached so we can restore it after Weakened expires
    float DefaultWalkSpeed = 0.f;
};