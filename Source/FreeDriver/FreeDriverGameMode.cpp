// Copyright Epic Games, Inc. All Rights Reserved.

#include "FreeDriverGameMode.h"
#include "FreeDriverPawn.h"
#include "FreeDriverHud.h"
#include "TP_ThirdPersonCharacter.h"
#include "UObject/ConstructorHelpers.h"

AFreeDriverGameMode::AFreeDriverGameMode()
{
	DefaultPawnClass = AFreeDriverPawn::StaticClass();
	HUDClass = AFreeDriverHud::StaticClass();
}

ATP_ThirdPersonGameMode::ATP_ThirdPersonGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}


