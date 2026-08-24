#pragma once
#include <BR-SDK.hpp>

namespace VehicleAPI
{
    //Returns whether or not the active vehicle changed. ONLY CALL AFTER VERIFYING THE VEHICLE
    bool SetActiveVehicle(SDK::ABrickVehicle* vehicle);
    //Vehicle is equal to player controller vehicle
    inline bool Valid(SDK::ABrickPlayerController* PC, SDK::ABrickVehicle* vehicle)
    {
        if (PC->PlayerVehicle == nullptr) SetActiveVehicle(nullptr);
        return PC && vehicle == PC->PlayerVehicle;
    }
}
