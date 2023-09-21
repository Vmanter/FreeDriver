// Fill out your copyright notice in the Description page of Project Settings.


#include "Pedestrian.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TP_ThirdPersonCharacter.h"
#include "FreeDriverPawn.h"

// Sets default values
APedestrian::APedestrian()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Health
	Health = 100;

}

// Called when the game starts or when spawned
void APedestrian::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APedestrian::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector toPlayer;
	float speedrun;
	//APedestrian* Player = Cast<APedestrian>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	ATP_ThirdPersonCharacter* Player = Cast<ATP_ThirdPersonCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Player) 
	{
		AFreeDriverPawn* CarPlayer = Cast<AFreeDriverPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
		if (!CarPlayer)
		{
			return;
		} else {
			toPlayer = CarPlayer->GetActorLocation() - GetActorLocation();
			speedrun = 50;
		}
	} else {
		toPlayer = Player->GetActorLocation() - GetActorLocation();
		speedrun = 20;
	}

	toPlayer.Normalize();

	AddMovementInput(toPlayer, speedrun * DeltaTime);

	FRotator toPlayerRotation = toPlayer.Rotation();
	toPlayerRotation.Pitch = 0; // 0 off the pitch
	RootComponent->SetWorldRotation(toPlayerRotation);

}

// Called to bind functionality to input
void APedestrian::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

