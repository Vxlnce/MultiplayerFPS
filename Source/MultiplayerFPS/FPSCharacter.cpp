// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSCharacter.h"

#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

#pragma region Overrides

// Sets default values
AFPSCharacter::AFPSCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Set up player camera
	Camera = CreateEditorOnlyDefaultSubobject<UCameraComponent>(TEXT("Camera"));

	// Rotate with player's mesh
	Camera->bUsePawnControlRotation = true;

	// Attach Camera to the socket in skeleton
	Camera->SetupAttachment(GetMesh(), "Camera");

	// Set movement constants
	GetCharacterMovement()->MaxWalkSpeed = 800.f;
	GetCharacterMovement()->JumpZVelocity = 600.f;
}

// Called when the game starts or when spawned
void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	// set off respawn sound
	UGameplayStatics::PlaySound2D(GetWorld(), SpawnSound);

	// Set up health and armor
	if(!HasAuthority()){ return; }

	SetHealth(MaxHealth);
	SetArmor(0);

	constexpr int32 WeaponCount = ENUM_TO_I32(EWeaponType::MAX);
	Weapons.Init(nullptr, WeaponCount);

	constexpr int32 AmmoCount = ENUM_TO_I32(EWeaponType::MAX);
	Ammo.Init(50, AmmoCount);

	for (int32 i = 0; i < WeaponCount; i++)
	{
		AddWeapon(static_cast<EWeaponType>(i));
	}

	EquipWeapon(EWeaponType::MachineGun, false);
}

// Called every frame
void AFPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AFPSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Rep health and armor
	DOREPLIFETIME_CONDITION(AFPSCharacter, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AFPSCharacter, Armor, COND_OwnerOnly);

	// Rep weaps
	DOREPLIFETIME_CONDITION(AFPSCharacter, Weapon, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AFPSCharacter, Weapons, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AFPSCharacter, Ammo, COND_OwnerOnly);
}

#pragma endregion

#pragma region Inputs
// Called to bind functionality to input
void AFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{

	// See if valid bindings
	check(PlayerInputComponent)

	// get our bindings and ad them to the mapping context
	UEnhancedInputComponent* EInpComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (EInpComponent == nullptr || PlayerController == nullptr)
	{
		return;
	}
	
	UEnhancedInputLocalPlayerSubsystem* EnhancedSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
	if(EnhancedSubsystem == nullptr)
	{
		return;
	}
	EnhancedSubsystem->AddMappingContext(IMC_Player, 1);
	
	EInpComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AFPSCharacter::PlayerInputMove);
	EInpComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AFPSCharacter::PlayerInputLook);
	EInpComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &AFPSCharacter::PlayerInputJump);

	EInpComponent->BindAction(IA_Fire, ETriggerEvent::Started, this, &AFPSCharacter::StartFire);
	EInpComponent->BindAction(IA_Fire, ETriggerEvent::Completed, this, &AFPSCharacter::StopFire);
	EInpComponent->BindAction(IA_Pistol, ETriggerEvent::Started, this, &AFPSCharacter::Pistol);
	EInpComponent->BindAction(IA_MachineGun, ETriggerEvent::Started, this, &AFPSCharacter::MachineGun);
	EInpComponent->BindAction(IA_Railgun, ETriggerEvent::Started, this, &AFPSCharacter::Railgun);
	EInpComponent->BindAction(IA_PrevWeapon, ETriggerEvent::Started, this, &AFPSCharacter::PrevWeapon);
	EInpComponent->BindAction(IA_NextWeapon, ETriggerEvent::Started,this, &AFPSCharacter::NextWeapon);
}

void AFPSCharacter::PlayerInputMove(const FInputActionValue& Value)
{

	// movement from mapping context
	const FVector2D InputValue = Value.Get<FVector2D>();
	
	// Got movement horizontally
	if (InputValue.X)
	{
		// Get right perpendicular vec to what camera is pointing at
		AddMovementInput(GetActorRightVector(), InputValue.X);
	}
	// Got movement forward
	if(InputValue.Y)
	{
		AddMovementInput(GetActorForwardVector(), InputValue.Y);
	}
}

void AFPSCharacter::PlayerInputLook(const FInputActionValue& Value)
{
	// movement from mapping context
	const FVector2D InputValue = Value.Get<FVector2D>();

	// horizontal mouse movement
	if (InputValue.X)
	{
		AddControllerYawInput(InputValue.X);
	}

	// vertical mouse movement
	if (InputValue.Y)
	{
		AddControllerPitchInput(-InputValue.Y);
	}
}

void AFPSCharacter::PlayerInputJump(const FInputActionValue& Value)
{
}

void AFPSCharacter::StartFire(const FInputActionValue& Value)
{
	if (Weapon)
	{
		Weapon->ServerStartFire();
	}
}

void AFPSCharacter::StopFire(const FInputActionValue& Value)
{
	if (Weapon)
{
	Weapon->ServerStopFire();
}
}

void AFPSCharacter::Pistol(const FInputActionValue& Value)
{
	ServerEquipWeapon(EWeaponType::Pistol);
}

void AFPSCharacter::MachineGun(const FInputActionValue& Value)
{
	ServerEquipWeapon(EWeaponType::MachineGun);
}

void AFPSCharacter::Railgun(const FInputActionValue& Value)
{
	ServerEquipWeapon(EWeaponType::Railgun);
}

void AFPSCharacter::NextWeapon(const FInputActionValue& Value)
{
	ServerCycleWeapons(1);
}

void AFPSCharacter::PrevWeapon(const FInputActionValue& Value)
{
	ServerCycleWeapons(-1);
}

FVector AFPSCharacter::GetCameraLocation() const
{
	return Camera->GetComponentLocation();
}

FVector AFPSCharacter::GetCameraDirection() const
{
	return GetControlRotation().Vector();
}

void AFPSCharacter::ServerEquipWeapon_Implementation(EWeaponType WeaponType)
{
	EquipWeapon(WeaponType);
}

void AFPSCharacter::ServerCycleWeapons_Implementation(int32 Direction)
{
	const int32 WeaponCount = Weapons.Num();
	const int32 StartWeaponIndex =
	GetSafeWrappedIndex(WeaponIndex,
	WeaponCount, Direction);
	for (int32 i = StartWeaponIndex; i != WeaponIndex; i = GetSafeWrappedIndex(i,WeaponCount, Direction))
	{
		if (EquipWeapon(static_cast<EWeaponType>(i)))
		{
			break;
		}
	}
}

#pragma endregion

#pragma region Armor

void AFPSCharacter::ApplyDamage(int Damage, AFPSCharacter* OtherCharacter)
{
	if (IsDead())
	{
		return;
	}
	
	ArmorAbsorbDamage(Damage);
	RemoveHealth(Damage);
	
	if (HitSound && OtherCharacter)
	{
		OtherCharacter->ClientPlaySound2D(HitSound);
	}
}

void AFPSCharacter::ArmorAbsorbDamage(int32& Damage)
{
	// make sure to at least absorb 1 damage when absorbing damage
	const int32 AbsorbedDamage = FMath::Clamp(Damage * ArmorAbsorption, 1, MAX_int32);

	// Set new armor durability
	const int32 RemainingArmor = Armor - AbsorbedDamage;
	SetArmor(RemainingArmor);

	// Make sure to deal at least 1 damage here
	Damage = FMath::Clamp(Damage * (1 - ArmorAbsorption), 1, MAX_int32);
}


#pragma endregion

#pragma region Generic RPCs

void AFPSCharacter::MulticastPlayAnimMontage_Implementation(UAnimMontage* AnimMontage)
{
	PlayAnimMontage(AnimMontage);
}

void AFPSCharacter::ClientPlaySound2D_Implementation(USoundBase* Sound)
{
	UGameplayStatics::PlaySound2D(GetWorld(), Sound);
}

#pragma endregion

#pragma region Weapons
void AFPSCharacter::AddWeapon(EWeaponType WeaponType)
{
	const int32 NewWeaponIndex = ENUM_TO_I32(WeaponType);
	if (!WeaponClasses.IsValidIndex(NewWeaponIndex) || Weapons[NewWeaponIndex])
	{
		return;
	}

	UClass* WeaponClass = WeaponClasses[NewWeaponIndex];
	if(!WeaponClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters = FActorSpawnParameters();

	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWeapon* NewWeapon = GetWorld()->SpawnActor<AWeapon>(WeaponClass, SpawnParameters);
	if (NewWeapon == nullptr)
	{
		return;
	}

	NewWeapon->SetActorHiddenInGame(true);
	Weapons[NewWeaponIndex] = NewWeapon;
	NewWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, "GripPoint");
}

bool AFPSCharacter::EquipWeapon(EWeaponType WeaponType, bool bPlaySound)
{
	const int32 NewWeaponIndex = ENUM_TO_I32(WeaponType);

	// check for valid index
	if(!Weapons.IsValidIndex(NewWeaponIndex))
	{
		return false;
	}

	AWeapon* NewWeapon = Weapons[NewWeaponIndex];

	if (NewWeapon == nullptr || Weapon == NewWeapon)
	{
		return false;
	}

	// hide the old weapon
	if (Weapon)
	{
		Weapon->SetActorHiddenInGame(true);
	}

	// show the new weapon
	Weapon = NewWeapon;
	Weapon->SetActorHiddenInGame(false);

	WeaponIndex = NewWeaponIndex;

	if (WeaponChangeSound && bPlaySound)
	{
		ClientPlaySound2D(WeaponChangeSound);
	}
	return true;
}

int32 AFPSCharacter::GetWeaponAmmo() const
{
	return Weapon != nullptr ? Ammo[ENUM_TO_I32(Weapon->GetAmmoType())] : 0;
}
#pragma endregion 
