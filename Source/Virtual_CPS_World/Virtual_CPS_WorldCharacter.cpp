// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Virtual_CPS_World.h"
#include "Virtual_CPS_WorldCharacter.h"

AVirtual_CPS_WorldCharacter::AVirtual_CPS_WorldCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Rotate character to moving direction
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	CameraBoom->bAbsoluteRotation = true; // Don't want arm to rotate when character does
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->RelativeRotation = FRotator(-60.f, 0.f, 0.f);
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
<<<<<<< HEAD
	TopDownCameraComponent->AttachToComponent(CameraBoom, FAttachmentTransformRules::KeepWorldTransform, USpringArmComponent::SocketName);
=======
	TopDownCameraComponent->AttachToComponent(CameraBoom, USpringArmComponent::SocketName);
>>>>>>> 70377ef466b566521cb7bf7d269563215dd47e88
	TopDownCameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

}
