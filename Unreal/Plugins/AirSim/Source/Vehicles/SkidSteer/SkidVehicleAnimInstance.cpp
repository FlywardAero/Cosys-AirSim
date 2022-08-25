// Based on the work by Leon Rosengarten and Boone Adkins.
// https://github.com/b-adkins/UE4-TankVehiclePlugin
// Developed by Cosys-Lab, University of Antwerp

#include "SkidVehicleAnimInstance.h"
#include "SkidVehicle.h"
#include "AnimationRuntime.h"

USkidVehicleAnimInstance::USkidVehicleAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

class ASkidVehicle* USkidVehicleAnimInstance::GetSkidVehicle()
{
	return Cast<ASkidVehicle>(GetOwningActor());
}