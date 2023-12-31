// Copyright Epic Games, Inc. All Rights Reserved.

#include "FreeDriverPawn.h"
#include "FreeDriverWheelFront.h"
#include "FreeDriverWheelRear.h"
#include "FreeDriverHud.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "WheeledVehicleMovementComponent4W.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"


#ifndef HMD_MODULE_INCLUDED
#define HMD_MODULE_INCLUDED 0
#endif

// Needed for VR Headset
#if HMD_MODULE_INCLUDED
#include "IXRTrackingSystem.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#endif // HMD_MODULE_INCLUDED

const FName AFreeDriverPawn::LookUpBinding("LookUp");
const FName AFreeDriverPawn::LookRightBinding("LookRight");
const FName AFreeDriverPawn::EngineAudioRPM("RPM");

#define LOCTEXT_NAMESPACE "VehiclePawn"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

AFreeDriverPawn::AFreeDriverPawn()
{
	// Car mesh
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(TEXT("/Game/VehicleAdv/Vehicle/Vehicle_SkelMesh.Vehicle_SkelMesh"));
	GetMesh()->SetSkeletalMesh(CarMesh.Object);
	CarInWorld = CarMesh.Object;
	
	static ConstructorHelpers::FClassFinder<UObject> AnimBPClass(TEXT("/Game/VehicleAdv/Vehicle/VehicleAnimationBlueprint"));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);

	// Setup friction materials
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> SlipperyMat(TEXT("/Game/VehicleAdv/PhysicsMaterials/Slippery.Slippery"));
	SlipperyMaterial = SlipperyMat.Object;
		
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> NonSlipperyMat(TEXT("/Game/VehicleAdv/PhysicsMaterials/NonSlippery.NonSlippery"));
	NonSlipperyMaterial = NonSlipperyMat.Object;

	UWheeledVehicleMovementComponent4W* Vehicle4W = CastChecked<UWheeledVehicleMovementComponent4W>(GetVehicleMovement());

	check(Vehicle4W->WheelSetups.Num() == 4);

	// Wheels/Tyres
	// Setup the wheels
	Vehicle4W->WheelSetups[0].WheelClass = UFreeDriverWheelFront::StaticClass();
	Vehicle4W->WheelSetups[0].BoneName = FName("PhysWheel_FL");
	Vehicle4W->WheelSetups[0].AdditionalOffset = FVector(0.f, -8.f, 0.f);

	Vehicle4W->WheelSetups[1].WheelClass = UFreeDriverWheelFront::StaticClass();
	Vehicle4W->WheelSetups[1].BoneName = FName("PhysWheel_FR");
	Vehicle4W->WheelSetups[1].AdditionalOffset = FVector(0.f, 8.f, 0.f);

	Vehicle4W->WheelSetups[2].WheelClass = UFreeDriverWheelRear::StaticClass();
	Vehicle4W->WheelSetups[2].BoneName = FName("PhysWheel_BL");
	Vehicle4W->WheelSetups[2].AdditionalOffset = FVector(0.f, -8.f, 0.f);

	Vehicle4W->WheelSetups[3].WheelClass = UFreeDriverWheelRear::StaticClass();
	Vehicle4W->WheelSetups[3].BoneName = FName("PhysWheel_BR");
	Vehicle4W->WheelSetups[3].AdditionalOffset = FVector(0.f, 8.f, 0.f);

	// Adjust the tire loading
	Vehicle4W->MinNormalizedTireLoad = 0.0f;
	Vehicle4W->MinNormalizedTireLoadFiltered = 0.2f;
	Vehicle4W->MaxNormalizedTireLoad = 2.0f;
	Vehicle4W->MaxNormalizedTireLoadFiltered = 2.0f;

	// Engine 
	// Torque setup
	Vehicle4W->MaxEngineRPM = 5700.0f;
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.0f, 400.0f);
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(1890.0f, 500.0f);
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(5730.0f, 400.0f);
 
	// Adjust the steering 
	Vehicle4W->SteeringCurve.GetRichCurve()->Reset();
	Vehicle4W->SteeringCurve.GetRichCurve()->AddKey(0.0f, 1.0f);
	Vehicle4W->SteeringCurve.GetRichCurve()->AddKey(40.0f, 0.7f);
	Vehicle4W->SteeringCurve.GetRichCurve()->AddKey(120.0f, 0.6f);
			
 	// Transmission	
	// We want 4wd
	Vehicle4W->DifferentialSetup.DifferentialType = EVehicleDifferential4W::LimitedSlip_4W;
	
	// Drive the front wheels a little more than the rear
	Vehicle4W->DifferentialSetup.FrontRearSplit = 0.65;

	// Automatic gearbox
	Vehicle4W->TransmissionSetup.bUseGearAutoBox = true;
	Vehicle4W->TransmissionSetup.GearSwitchTime = 0.15f;
	Vehicle4W->TransmissionSetup.GearAutoBoxLatency = 1.0f;

	// Physics settings
	// Adjust the center of mass - the buggy is quite low
	UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(Vehicle4W->UpdatedComponent);
	if (UpdatedPrimitive)
	{
		UpdatedPrimitive->BodyInstance.COMNudge = FVector(8.0f, 0.0f, 0.0f);
	}

	// Set the inertia scale. This controls how the mass of the vehicle is distributed.
	Vehicle4W->InertiaTensorScale = FVector(1.0f, 2.333f, 1.2f);

	// Create a spring arm component for our chase camera
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 34.0f));
	SpringArm->SetWorldRotation(FRotator(-20.0f, 0.0f, 0.0f));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 125.0f;
	SpringArm->bEnableCameraLag = true;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = true;

	// Create the chase camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetRelativeLocation(FVector(-125.0, 0.0f, 0.0f));
	Camera->SetRelativeRotation(FRotator(10.0f, 0.0f, 0.0f));
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 90.f;

	// Create a spring arm In-Car component for our chase camera
	SpringArmInCar = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmInCar"));
	SpringArmInCar->SetRelativeLocation(FVector(-34.0f, -10.0f, 20.0f));
	SpringArmInCar->SetWorldRotation(FRotator(-10.0f, 0.0f, 0.0f));
	SpringArmInCar->SetupAttachment(RootComponent);
	SpringArmInCar->TargetArmLength = 15.0f;
	SpringArmInCar->bEnableCameraLag = true; // false;
	SpringArmInCar->bEnableCameraRotationLag = true;
	SpringArmInCar->bInheritPitch = true;
	SpringArmInCar->bInheritYaw = true;
	SpringArmInCar->bInheritRoll = true;


	// Create In-Car camera component 
	InternalCameraOrigin = FVector(-34.0f, -10.0f, 50.0f);
	InternalCameraBase = CreateDefaultSubobject<USceneComponent>(TEXT("InternalCameraBase"));
	InternalCameraBase->SetupAttachment(SpringArmInCar, USpringArmComponent::SocketName);
	InternalCameraBase->SetRelativeLocation(InternalCameraOrigin);
	//InternalCameraBase->SetupAttachment(GetMesh());

	InternalCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InternalCamera"));
	InternalCamera->bUsePawnControlRotation = false;
	InternalCamera->FieldOfView = 75.f;
	InternalCamera->SetupAttachment(InternalCameraBase);

	// In car HUD
	// Create text render component for in car speed display
	InCarSpeed = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarSpeed"));
	InCarSpeed->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));
	InCarSpeed->SetRelativeLocation(FVector(35.0f, -6.0f, 20.0f));
	InCarSpeed->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	InCarSpeed->SetupAttachment(GetMesh());

	// Create text render component for in car gear display
	InCarGear = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
	InCarGear->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));
	InCarGear->SetRelativeLocation(FVector(35.0f, 5.0f, 20.0f));
	InCarGear->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	InCarGear->SetupAttachment(GetMesh());
	
	// Setup the audio component and allocate it a sound cue
	static ConstructorHelpers::FObjectFinder<USoundCue> SoundCue(TEXT("/Game/VehicleAdv/Sound/Engine_Loop_Cue.Engine_Loop_Cue"));
	EngineSoundComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineSound"));
	EngineSoundComponent->SetSound(SoundCue.Object);
	EngineSoundComponent->SetupAttachment(GetMesh());

	// Colors for the in-car gear display. One for normal one for reverse
	GearDisplayReverseColor = FColor(255, 0, 0, 255);
	GearDisplayColor = FColor(255, 255, 255, 255);

	bIsLowFriction = false;
	bInReverseGear = false;
}

void AFreeDriverPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// set up gameplay key bindings
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AFreeDriverPawn::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AFreeDriverPawn::MoveRight);
	PlayerInputComponent->BindAxis(LookUpBinding);
	PlayerInputComponent->BindAxis(LookRightBinding);

	PlayerInputComponent->BindAction("Handbrake", IE_Pressed, this, &AFreeDriverPawn::OnHandbrakePressed);
	PlayerInputComponent->BindAction("Handbrake", IE_Released, this, &AFreeDriverPawn::OnHandbrakeReleased);
	PlayerInputComponent->BindAction("SwitchCamera", IE_Pressed, this, &AFreeDriverPawn::OnToggleCamera);

	PlayerInputComponent->BindAction("EnterVehicle", IE_Pressed, this, &AFreeDriverPawn::ExitCharacter);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AFreeDriverPawn::OnResetVR); 
}

void AFreeDriverPawn::MoveForward(float Val)
{
	GetVehicleMovementComponent()->SetThrottleInput(Val);

}

void AFreeDriverPawn::MoveRight(float Val)
{
	GetVehicleMovementComponent()->SetSteeringInput(Val);
}

void AFreeDriverPawn::EnterCharacter(ATP_ThirdPersonCharacter* Character) 
{
	if (Character) {
		OwnerCharacter = Character;
		Character->CharacterMovement->StopMovementImmediately();
		Character->SetActorEnableCollision(false);
		//Character->K2_AttachRootComponentTo(CarInWorld, NAME_None, EAttachLocation::SnapToTarget);
		//Character->
		//Character->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepWorldTransform, FName(TEXT("Drive now!")));
		//Character->K2_AttachRootComponentTo(GetMesh(), NAME_None, EAttachLocation::SnapToTarget);

		//SpringArmSeatDriverCar = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmSeatDriverCar"));
		//SpringArmSeatDriverCar->SetRelativeLocation(FVector(-34.0f, -10.0f, 20.0f));
		//SpringArmSeatDriverCar->SetWorldRotation(FRotator(-10.0f, 0.0f, 0.0f));
		//SpringArmSeatDriverCar->SetupAttachment(RootComponent, "DriverSocket");
		//SpringArmSeatDriverCar->TargetArmLength = 15.0f;
		//SpringArmSeatDriverCar->bEnableCameraLag = true; // false;
		//SpringArmSeatDriverCar->bEnableCameraRotationLag = true;
		//SpringArmSeatDriverCar->bInheritPitch = true;
		//SpringArmSeatDriverCar->bInheritYaw = true;
		//SpringArmSeatDriverCar->bInheritRoll = true;


		// Create In-Car camera component 
		//InternalCameraOrigin = FVector(-34.0f, -10.0f, 50.0f);
		//InternalCameraBase = CreateDefaultSubobject<USceneComponent>(TEXT("InternalCameraBase"));
		//InternalCameraBase->SetupAttachment(SpringArmInCar, USpringArmComponent::SocketName);
		//InternalCameraBase->SetRelativeLocation(InternalCameraOrigin);

		//Character->K2_AttachRootComponentTo(SpringArmSeatDriverCar, USpringArmComponent::SocketName, EAttachLocation::SnapToTarget);
		//Character->SetRelativeLocation(FVector(-10.0f, -10.0f, 10.0f));
		Character->K2_AttachRootComponentTo(GetMesh(), "DriverSocket", EAttachLocation::SnapToTarget);
		Character->inCar = true;
		if (Character->Controller)
		{
			Character->Controller->Possess(this);
		}
	}
}

void AFreeDriverPawn::ExitCharacter()
{
	//this->SetActorEnableCollision(false);
	//this->AddActorLocalOffset(FVector(10.f, -400.f, 0.f), false, 0, ETeleportType::ResetPhysics);
	
	if (OwnerCharacter) {
		OwnerCharacter->DetachRootComponentFromParent();
		//OwnerCharacter->AddActorLocalOffset(FVector(10.f, 400.f, 100.f));
		OwnerCharacter->AddActorLocalOffset(FVector(0.f, -100.f, 0.f), false, 0, ETeleportType::ResetPhysics);
		//this->SetActorEnableCollision(false);
		OwnerCharacter->SetActorRotation(FRotator::ZeroRotator);
		//OwnerCharacter->SetActorRotation(FRotator::Normalize);
 		OwnerCharacter->SetActorEnableCollision(true);
		OwnerCharacter->inCar = false;

		EnableIncarView(false);

		if (Controller) {
			Controller->Possess(OwnerCharacter);
			//OwnerCharacter->SetActorEnableCollision(true);
			OwnerCharacter = NULL;
		}

		//this->SetActorEnableCollision(true);
	}

	//_sleep(1000);
	//this->SetActorEnableCollision(true);
}

void AFreeDriverPawn::OnHandbrakePressed()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(true);
}

void AFreeDriverPawn::OnHandbrakeReleased()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(false);
}

void AFreeDriverPawn::OnToggleCamera()
{
	EnableIncarView(!bInCarCameraActive);
}

void AFreeDriverPawn::EnableIncarView(const bool bState)
{
	if (bState != bInCarCameraActive)
	{
		bInCarCameraActive = bState;
		
		if (bState == true)
		{
			OnResetVR();
			Camera->Deactivate();
			OwnerCharacter->SetActorHiddenInGame(true);
			InternalCamera->Activate();
		}
		else
		{
			InternalCamera->Deactivate();
			OwnerCharacter->SetActorHiddenInGame(false);
			Camera->Activate();
		}
		
		InCarSpeed->SetVisibility(bInCarCameraActive);
		InCarGear->SetVisibility(bInCarCameraActive);
	}
}

void AFreeDriverPawn::Tick(float Delta)
{
	Super::Tick(Delta);

	// Setup the flag to say we are in reverse gear
	bInReverseGear = GetVehicleMovement()->GetCurrentGear() < 0;
	
	// Update phsyics material
	UpdatePhysicsMaterial();

	// Update the strings used in the hud (incar and onscreen)
	UpdateHUDStrings();

	// Set the string in the incar hud
	SetupInCarHUD();

	bool bHMDActive = false;
#if HMD_MODULE_INCLUDED
	if ((GEngine->XRSystem.IsValid() == true ) && ( (GEngine->XRSystem->IsHeadTrackingAllowed() == true) || (GEngine->IsStereoscopic3D() == true)))
	{
		bHMDActive = true;
	}
#endif // HMD_MODULE_INCLUDED
	if( bHMDActive == false )
	{
		if ( (InputComponent) && (bInCarCameraActive == true ))
		{
			FRotator HeadRotation = InternalCamera->GetRelativeRotation();
			HeadRotation.Pitch -= InputComponent->GetAxisValue(LookUpBinding);
			HeadRotation.Yaw += InputComponent->GetAxisValue(LookRightBinding);
			InternalCamera->SetRelativeRotation(HeadRotation);
		}
	}	

	// Pass the engine RPM to the sound component
	float RPMToAudioScale = 2500.0f / GetVehicleMovement()->GetEngineMaxRotationSpeed();
	EngineSoundComponent->SetFloatParameter(EngineAudioRPM, GetVehicleMovement()->GetEngineRotationSpeed()*RPMToAudioScale);
}

void AFreeDriverPawn::BeginPlay()
{
	Super::BeginPlay();

	bool bWantInCar = false;
	// First disable both speed/gear displays 
	bInCarCameraActive = false;
	InCarSpeed->SetVisibility(bInCarCameraActive);
	InCarGear->SetVisibility(bInCarCameraActive);

	// Enable in car view if HMD is attached
#if HMD_MODULE_INCLUDED
	bWantInCar = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
#endif // HMD_MODULE_INCLUDED

	EnableIncarView(bWantInCar);
	// Start an engine sound playing
	EngineSoundComponent->Play();
}

void AFreeDriverPawn::OnResetVR()
{
#if HMD_MODULE_INCLUDED
	if (GEngine->XRSystem.IsValid())
	{
		GEngine->XRSystem->ResetOrientationAndPosition();
		InternalCamera->SetRelativeLocation(InternalCameraOrigin);
		GetController()->SetControlRotation(FRotator());
	}
#endif // HMD_MODULE_INCLUDED
}

void AFreeDriverPawn::UpdateHUDStrings()
{
	float KPH = FMath::Abs(GetVehicleMovement()->GetForwardSpeed()) * 0.036f;
	int32 KPH_int = FMath::FloorToInt(KPH);
	int32 Gear = GetVehicleMovement()->GetCurrentGear();

	// Using FText because this is display text that should be localizable
	SpeedDisplayString = FText::Format(LOCTEXT("SpeedFormat", "{0} km/h"), FText::AsNumber(KPH_int));


	if (bInReverseGear == true)
	{
		GearDisplayString = FText(LOCTEXT("ReverseGear", "R"));
	}
	else
	{
		GearDisplayString = (Gear == 0) ? LOCTEXT("N", "N") : FText::AsNumber(Gear);
	}

}

void AFreeDriverPawn::SetupInCarHUD()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if ((PlayerController != nullptr) && (InCarSpeed != nullptr) && (InCarGear != nullptr))
	{
		// Setup the text render component strings
		InCarSpeed->SetText(SpeedDisplayString);
		InCarGear->SetText(GearDisplayString);
		
		if (bInReverseGear == false)
		{
			InCarGear->SetTextRenderColor(GearDisplayColor);
		}
		else
		{
			InCarGear->SetTextRenderColor(GearDisplayReverseColor);
		}
	}
}

void AFreeDriverPawn::UpdatePhysicsMaterial()
{
	if (GetActorUpVector().Z < 0)
	{
		if (bIsLowFriction == true)
		{
			GetMesh()->SetPhysMaterialOverride(NonSlipperyMaterial);
			bIsLowFriction = false;
		}
		else
		{
			GetMesh()->SetPhysMaterialOverride(SlipperyMaterial);
			bIsLowFriction = true;
		}
	}
}

#undef LOCTEXT_NAMESPACE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
