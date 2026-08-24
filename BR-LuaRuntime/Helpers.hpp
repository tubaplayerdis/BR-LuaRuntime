#pragma once
#include <string>
#include <BR-SDK.hpp>

namespace Helpers
{
    std::wstring to_wstring(const std::string& str);
    DWORD_PTR GetStaticAddressFromVA(PVOID va);
    SDK::ABrickPlayerController* GetBrickPlayerController();
    void SendUserError(const std::string& errorMessage);
    void SendUserError(const std::wstring& context, const std::string& errorMessage);
    bool IsLuaBrick(SDK::UBrick* Brick);
    void PrintWholeString(std::wstring str);
    std::string FindModuleSource(SDK::ABrickVehicle* Vehicle, const std::string& moduleName);
}
