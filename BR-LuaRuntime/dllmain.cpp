// dllmain.cpp : Defines the entry point for the DLL application.
#define WIN32_LEAN_AND_MEAN             
#include <windows.h>
#include <Hooking/MinHook/MinHook.h>
#include <BR-SDK.hpp>
#include "LuaRuntime.hpp"

/*
* HOW 2 ADD LUA TO Brick Rigs?
* 
* 1. Find a viable brick type to use as a base for a lua brick. It needs string input and ability to read multiple input channels and an output. - Done
* 2. Have a custom static info type with BRMK. That will be the starting point for our mod. - Done
* 3. Override the beginplay, brick tick and the should brick tick for all switch bricks.
* 4. Have a global lua state for the client.
* 5. Configure global lua state with bindings to get input and output.
* 6. Execute lua strings of bricks on tick with the context of the brick pointer being changed for each brick.
*/

/*
* Lua code - string
* inputs
* output 
*/

/*
* 
*/

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
    SetConsoleTitleW(L"Brick Rigs Lua Runtime - Debug");
    SetConsoleOutputCP(CP_UTF8);
#endif // _DEBUG

    MH_Initialize(); //Initalize MinHook
	LuaRuntime::Initialize(); //Initalize Lua Runtime

    while (true)
    {
        if (GetAsyncKeyState(VK_F6) & 0x8000)
        {
            CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr);
            return 0;
        }

		Sleep(100);
    }

    
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

std::wstring to_wstring(const std::string& str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    wstr.pop_back(); // remove null terminator
    return wstr;
}
