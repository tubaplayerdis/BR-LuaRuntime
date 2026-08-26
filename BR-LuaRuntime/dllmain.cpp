// dllmain.cpp : Defines the entry point for the DLL application.
#define WIN32_LEAN_AND_MEAN             
#include <windows.h>
#include <Hooking/MinHook/MinHook.h>
#include <BR-SDK.hpp>
#include <Platform/Platform.h>

#include "Helpers.hpp"
#include "LuaRuntime.hpp"

extern int O_GWorld;
UC::int32 OGWorld()
{
    using namespace SDK;
    for (int i = 0; i < UObject::GObjects->Num(); i++)
    {
        UObject* Obj = UObject::GObjects->GetByIndex(i);

        if (!Obj)
            continue;

        if (!Obj->IsA(UWorld::StaticClass()) || Obj->IsDefaultObject())
            continue;

        auto Results = Platform::FindAllAlignedValuesInProcess(Obj);

        if (Results.empty())
            continue; // this UWorld instance has no live pointer to it, try the next one

        std::cout << Results.size() << " Num results" << std::endl;

        void* Result = nullptr;


        if (Results.size() == 1)
        {
            Result = Results[0];
        }
        else if (Results.size() == 2)
        {
            auto ObjAddress = reinterpret_cast<uintptr_t>(Obj);
            auto PossibleGWorld = reinterpret_cast<volatile uintptr_t*>(Results[0]);
            auto CurrentValue = *PossibleGWorld;

            for (int j = 0; CurrentValue == ObjAddress && j < 50; ++j)
            {
                ::Sleep(1);
                CurrentValue = *PossibleGWorld;
            }

            if (CurrentValue == ObjAddress)
            {
                Result = Results[0];
            }
            else
            {
                Result = Results[1];
                std::cerr << std::format("Filter GActiveLogWorld at 0x{:X}\n\n", reinterpret_cast<uintptr_t>(PossibleGWorld));
            }
        }
        else
        {
            std::cerr << std::format("Detected {} candidates for GWorld, skipping this object\n\n", Results.size());
            continue; // ambiguous — don't guess, try another UWorld instance instead of bailing entirely
        }


        if (Result)
        {
            O_GWorld = static_cast<int32>(Helpers::GetStaticAddressFromVA(Result));
            break; // found it — stop scanning immediately
        }
    }

    if (O_GWorld == 0)
        std::cerr << "GWorld offset NOT FOUND" << std::endl;

    return O_GWorld;
}


//Global variables
HMODULE self = nullptr;
FILE* pStdIn = nullptr;
FILE* pStdOut = nullptr;
FILE* pStdErr = nullptr;
PVOID pHandleVec = nullptr;

//Definied in execption_handler.cpp
LONG WINAPI UpgradedExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo);

//Safley unloads the mod. Delay added to help with execution.
DWORD WINAPI UnloadThread(LPVOID lpParam) {
    Sleep(10);
    FreeLibraryAndExitThread(self, 0);
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
    HMODULE hModule = static_cast<HMODULE>(lpReserved);
    self = hModule;

#ifdef _DEBUG //If in debug version enable console.
    AllocConsole();
    freopen_s(&pStdIn, "CONIN$", "r", stdin);
    freopen_s(&pStdOut, "CONOUT$", "w", stdout);
    freopen_s(&pStdErr, "CONOUT$", "w", stderr);
    SetConsoleTitleW(L"Brick Rigs Lua Runtime - Developer");
    SetConsoleOutputCP(CP_UTF8);
#endif // _DEBUG

#ifdef _DEBUG
    std::cout << "Brick Rigs Lua Runtime - American_Stig (tbgit) @Discord" << std::endl;
    std::cout << "API Reference: " << "https://github.com/tubaplayerdis/BR-LuaRuntime" << std::endl;
    std::cout << "Lua runtime will cause FREEZES Sometimes - Press ENTER to fix" << std::endl;
    std::cout << "Lua runtime is in developer mode - Press F6 to uninject" << std::endl;
#endif

    MH_Initialize(); //Initalize MinHook
    BR_SDK_Init();
    LuaRuntime::Initialize(); //Initalize Lua Runtime

#ifdef _DEBUG
    while (true)
    {
        if (GetAsyncKeyState(VK_F6) & 0x8000)
        {
            CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr);
            return 0;
        }

		Sleep(10);
    }
#endif

    return 0;
}

void CleanUp(HMODULE hModule)
{
	LuaRuntime::Shutdown();

    MH_DisableHook(MH_ALL_HOOKS);
    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();

#ifdef _DEBUG
    fclose(pStdIn);
    fclose(pStdOut);
    fclose(pStdErr);
    SetStdHandle(STD_INPUT_HANDLE, nullptr);
    SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
    SetStdHandle(STD_ERROR_HANDLE, nullptr);
    FreeConsole();
    PostMessage(GetConsoleWindow(), WM_CLOSE, 0, 0);
#endif

    RemoveVectoredExceptionHandler(pHandleVec);
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        pHandleVec = AddVectoredExceptionHandler(1, UpgradedExceptionHandler);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }

    if (reason == DLL_PROCESS_DETACH)
    {
        CleanUp(self);
    }
    return TRUE;
}
