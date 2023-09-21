// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FreeDriverHud.generated.h"

UCLASS(config = Game)
class AFreeDriverHud : public AHUD
{
	GENERATED_BODY()

public:
	AFreeDriverHud();

	/** Font used to render the vehicle info */
	UPROPERTY()
	UFont* HUDFont;

	// Begin AHUD interface
	virtual void DrawHUD() override;
	// End AHUD interface

};
