#include "Helpers.hpp"
#include <string>

std::wstring Helpers::to_wstring(const std::string& str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    wstr.pop_back(); // remove null terminator
    return wstr;
}

SDK::ABrickPlayerController* Helpers::GetBrickPlayerController()
{
    SDK::APlayerController* PlayerController = SDK::UGameplayStatics::GetPlayerController(SDK::UWorld::GetWorld(), 0);
    if (PlayerController)
    {
        return static_cast<SDK::ABrickPlayerController*>(PlayerController);
    }
    return nullptr;
}

Function<void(SDK::FBrickChatMessage* This, SDK::EChatMessageType Type, SDK::ABrickPlayerController* Controller)> FBrickChatMessageConstructor("48 89 5C 24 08 57 48 83 EC 20 88");

void Helpers::SendUserError(const std::string& errorMessage)
{
    auto PC = GetBrickPlayerController();
    if (!PC) return;
    SDK::FBrickChatMessage Message;
    FBrickChatMessageConstructor(&Message, SDK::EChatMessageType::Message, PC);
    std::wstring ErrorWString = to_wstring(errorMessage);
    SDK::FString ErrorString(ErrorWString.c_str());
    Message.TextOption = SDK::UKismetTextLibrary::Conv_StringToText(ErrorString);
    Message.Player.PlayerName = UC::FString(L"Lua");
    Message.Type = SDK::EChatMessageType::Message;
    PC->ClientReceiveChatMessage(Message);
}

void Helpers::SendUserError(const std::wstring& context, const std::string& errorMessage)
{
    auto PC = GetBrickPlayerController();
    if (!PC) return;
    SDK::FBrickChatMessage Message;
    FBrickChatMessageConstructor(&Message, SDK::EChatMessageType::Message, PC);
    std::wstring ErrorWString = to_wstring(errorMessage);
    SDK::FString ErrorString(ErrorWString.c_str());
    std::wstring PlayerContext = L"Lua (" + context + L")";
    Message.TextOption = SDK::UKismetTextLibrary::Conv_StringToText(ErrorString);
    Message.Player.PlayerName = UC::FString(PlayerContext.c_str());
    Message.Type = SDK::EChatMessageType::Message;
    PC->ClientReceiveChatMessage(Message);
}

bool Helpers::IsLuaBrick(SDK::UBrick* Brick)
{
    auto SI = Brick->GetStaticInfo();
    bool IsLuaBrick = Brick
    && SI->IsA(SDK::USwitchBrickStaticInfo::StaticClass())
    && (SI->GetName() == "Default__BP_LuaBrick_C"
        || reinterpret_cast<SDK::USwitchBrick*>(Brick)->SwitchName.ToString().starts_with('='));
    return IsLuaBrick;
}

void Helpers::PrintWholeString(std::wstring str)
{
    for (wchar_t c : str) std::cout << (int)c << " ";
    std::cout << "\n";
}

std::string Helpers::FindModuleSource(SDK::ABrickVehicle* Vehicle, const std::string& moduleName)
{
    std::cout << "Finding module: " << moduleName << std::endl;

    std::wstring wantedId(moduleName.begin(), moduleName.end()); // adjust for real wide conversion
    for (SDK::UBrick* Brick : Vehicle->GetBricks())
    {
        if (!Brick->IsA(SDK::UTextBrick::StaticClass())) continue;
        auto TextBrick = reinterpret_cast<SDK::UTextBrick*>(Brick);
        std::string BrickText = TextBrick->Text.ToString();
        if (!BrickText.starts_with('=')) continue;
        std::wstring text = Helpers::to_wstring(BrickText);
        size_t nl = text.find(L'\n');
        size_t eq = text.find(L'=');
        if (nl == std::wstring::npos) continue;
        std::wstring id = text.substr(eq+1, nl-2);//new carriage + newline
        std::wcout << "Checking against module: " << id << std::endl;
        if (id == wantedId) // reuse your existing ID-line convention
        {
            std::cout << "Found Module: " << moduleName << std::endl;
            return BrickText.substr(BrickText.find('\n') + 1);
        }
    }

    std::cout << "Did not find module!" << moduleName << std::endl;
    return "";
}
