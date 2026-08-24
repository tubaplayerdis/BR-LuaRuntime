#include "VehicleAPI.hpp"

#include "Helpers.hpp"

SDK::ABrickVehicle* ActiveVehicle = nullptr;

bool VehicleAPI::SetActiveVehicle(SDK::ABrickVehicle* vehicle)
{
    if (vehicle != ActiveVehicle)
    {
        ActiveVehicle = vehicle;
        return true;
    }
    return false;
}
