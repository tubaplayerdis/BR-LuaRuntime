// dllmain.cpp : Defines the entry point for the DLL application.
#define WIN32_LEAN_AND_MEAN             
#include <windows.h>
#include <Hooking/MinHook/MinHook.h>
#include <BR-SDK.hpp>
#include <Platform/Platform.h>

#include "Helpers.hpp"
#include "LuaRuntime.hpp"


//Global variables
HMODULE self = nullptr;
FILE* pStdIn = nullptr;
FILE* pStdOut = nullptr;
FILE* pStdErr = nullptr;
PVOID pHandleVec = nullptr;
HANDLE hShutdownEvent = nullptr;

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

    while (true)
    {
#ifdef _DEBUG
        if (GetAsyncKeyState(VK_F6) & 0x8000)
        {
            break;
        }
#endif

		if (WaitForSingleObject(hShutdownEvent, 10))
		{
		    break;
		}
    }

    CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr);
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

        wchar_t eventName[64];
        swprintf_s(eventName, L"Unload_%lu", GetCurrentProcessId());
        hShutdownEvent = CreateEventW(NULL, TRUE, FALSE, eventName);
    }

    if (reason == DLL_PROCESS_DETACH)
    {
        CleanUp(self);
    }
    return TRUE;
}
