#include "BossCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"

ABossCharacter::ABossCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ABossCharacter::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;
    CurrentPoise = MaxPoise;

    // Cache default walk speed so we can restore it after Weakened expires
    if (GetCharacterMovement())
    {
        DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
    }

    // Start enrage countdown
    GetWorldTimerManager().SetTimer(
        EnrageTimer,
        this,
        &ABossCharacter::TriggerEnrage,
        EnrageTime,
        false
    );
}

void ABossCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// --- TakeDamage --------------------------------------------------------------
// Order of operations:
//   1. Ignore if already dead
//   2. Apply shell armor if hit from the front
//   3. Apply incoming damage multiplier (x2 when dish is empty)
//   4. Subtract from health
//   5. Check for phase transition
//   6. Check for death

float ABossCharacter::TakeDamage(float DamageAmount,
    struct FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    if (bIsDead) return 0.f;

    float FinalDamage = DamageAmount;

    // Shell armor check - only works for point damage (melee/projectile hits)
    if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
    {
        const FPointDamageEvent* PointEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
        FinalDamage = ApplyShellArmor(FinalDamage, PointEvent->ShotDirection);
    }

    // Incoming multiplier (Weakened state doubles all damage taken)
    FinalDamage *= IncomingDamageMultiplier;

    CurrentHealth = FMath::Max(CurrentHealth - FinalDamage, 0.f);

    CheckPhaseTransition();

    if (CurrentHealth <= 0.f)
    {
        HandleDeath();
    }

    return FinalDamage;
}

// --- ReceiveBodyPartHit ------------------------------------------------------
// Called directly by box components on the boss mesh.
// Routes each body part to the correct behaviour before applying damage.

void ABossCharacter::ReceiveBodyPartHit(EBodyPart BodyPart, AActor* DamageCauser, float RawDamage)
{
    if (bIsDead) return;

    switch (BodyPart)
    {
    case EBodyPart::Head:
        DrainDish(DishDrainPerHit);
        break;

    case EBodyPart::Shell:
        // Armor multiplier is handled inside TakeDamage via dot product check.
        // Nothing extra needed here.
        break;

    case EBodyPart::LeftClaw:
    case EBodyPart::RightClaw:
        // Claw hits deal full damage - no modifier.
        break;

    default:
        break;
    }

    UGameplayStatics::ApplyDamage(this, RawDamage, nullptr, DamageCauser, nullptr);
}

// --- ApplyShellArmor ---------------------------------------------------------
// Checks whether the hit came from the front arc using a dot product.
// HitDirection points FROM attacker TO boss, so we negate it.

float ABossCharacter::ApplyShellArmor(float RawDamage, const FVector& HitDirection)
{
    FVector ToAttacker = -HitDirection.GetSafeNormal();
    float Dot = FVector::DotProduct(ToAttacker, GetActorForwardVector());

    if (Dot > ShellArmorArcThreshold)
    {
        return RawDamage * ShellArmorMultiplier;
    }

    return RawDamage;
}

// --- DrainDish ---------------------------------------------------------------

void ABossCharacter::DrainDish(float Amount)
{
    if (bIsDead) return;

    WaterLevel = FMath::Clamp(WaterLevel - Amount, 0.f, 1.f);

    if (WaterLevel <= 0.f && !bIsWeakened)
    {
        bIsWeakened = true;
        IncomingDamageMultiplier = WeakenedDamageMultiplier;

        if (GetCharacterMovement())
        {
            GetCharacterMovement()->MaxWalkSpeed *= WeakenedSpeedMultiplier;
        }

        OnDishEmptied();
        OnDishDrained.Broadcast();

        GetWorldTimerManager().SetTimer(
            WeakenedTimer,
            this,
            &ABossCharacter::EndWeakened,
            WeakenedDuration,
            false
        );
    }
}

// --- EndWeakened -------------------------------------------------------------

void ABossCharacter::EndWeakened()
{
    bIsWeakened = false;
    IncomingDamageMultiplier = 1.f;

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
    }

    // Start the dish refill timer
    GetWorldTimerManager().SetTimer(
        DishRefillTimer,
        this,
        &ABossCharacter::RefillDish,
        DishRefillTime,
        false
    );
}

// --- RefillDish --------------------------------------------------------------

void ABossCharacter::RefillDish()
{
    WaterLevel = 1.f;
}

// --- ApplyPoiseHit -----------------------------------------------------------

void ABossCharacter::ApplyPoiseHit(float HitImpulse, bool bBypass)
{
    if (bIsDead) return;

    if (bBypass)
    {
        CurrentPoise = 0.f;
    }
    else
    {
        CurrentPoise = FMath::Max(CurrentPoise - HitImpulse, 0.f);
    }

    if (CurrentPoise <= 0.f)
    {
        OnStaggerVisuals();
        OnBossStagger.Broadcast();

        GetWorldTimerManager().SetTimer(
            PoiseResetTimer,
            this,
            &ABossCharacter::ResetPoise,
            PoiseResetTime,
            false
        );
    }
}

// --- ResetPoise --------------------------------------------------------------

void ABossCharacter::ResetPoise()
{
    CurrentPoise = MaxPoise;
}

// --- CheckPhaseTransition ----------------------------------------------------

void ABossCharacter::CheckPhaseTransition()
{
    float HPRatio = CurrentHealth / MaxHealth;

    if (CurrentPhase == EBossPhase::Phase1 && HPRatio <= Phase2Threshold)
    {
        CurrentPhase = EBossPhase::Phase2;
        OnPhaseTransitionVisuals(CurrentPhase);
        OnPhaseChanged.Broadcast(CurrentPhase);
    }
    else if (CurrentPhase == EBossPhase::Phase2 && HPRatio <= Phase3Threshold)
    {
        CurrentPhase = EBossPhase::Phase3;
        OnPhaseTransitionVisuals(CurrentPhase);
        OnPhaseChanged.Broadcast(CurrentPhase);
    }
}

// --- HandleDeath -------------------------------------------------------------

void ABossCharacter::HandleDeath()
{
    if (bIsDead) return;
    bIsDead = true;

    GetWorldTimerManager().ClearTimer(PoiseResetTimer);
    GetWorldTimerManager().ClearTimer(DishRefillTimer);
    GetWorldTimerManager().ClearTimer(WeakenedTimer);
    GetWorldTimerManager().ClearTimer(EnrageTimer);

    SetActorEnableCollision(false);

    OnDeathVisuals();
    OnBossDeath.Broadcast();
}

// --- TriggerEnrage -----------------------------------------------------------

void ABossCharacter::TriggerEnrage()
{
    if (bIsDead) return;

    bIsEnraged = true;

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed *= 1.3f;
    }

    OnEnrageVisuals();
}

// --- CompleteIntro -----------------------------------------------------------

void ABossCharacter::CompleteIntro()
{
    bIntroComplete = true;
}