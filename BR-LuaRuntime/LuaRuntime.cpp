#include "LuaRuntime.hpp"
#include <Lua/lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include <BR-SDK.hpp>

#define LUA_MAX_INSTRUCTIONS_PER_TICK 10000

std::wstring to_wstring(const std::string& str);
DWORD_PTR GetStaticAddressFromVA(PVOID va);

SDK::ABrickPlayerController* GetBrickPlayerController()
{
	SDK::APlayerController* PlayerController = SDK::UGameplayStatics::GetPlayerController(SDK::UWorld::GetWorld(), 0);
	if (PlayerController)
	{
		return static_cast<SDK::ABrickPlayerController*>(PlayerController);
	}
	return nullptr;
}

bool IsLuaBrick(SDK::UBrick* Brick)
{
    auto SI = Brick->GetStaticInfo();
    bool IsLuaBrick = Brick && SI->IsA(SDK::USwitchBrickStaticInfo::StaticClass()) && SI->GetName() == "Default__BP_LuaBrick_C";
    std::cout << "Lua Brick!" << std::endl;
    return IsLuaBrick;
}

lua_State* L = nullptr;

struct LuaBrick
{
    lua_State* Coroutine;                // raw pointer for fast lookup/dispatch
    luabridge::LuaRef ThreadRef;         // keeps the coroutine alive (was: int threadRef + luaL_ref/unref)
    luabridge::LuaRef EnvRef;            // this brick's private _ENV table
    SDK::USwitchBrick* Brick;
    bool HasError = false;
};
SDK::USwitchBrick* ActiveBrick = nullptr;

std::unordered_map<SDK::USwitchBrick*, LuaBrick> LuaBricks;       // owns the contexts, node-stable
std::unordered_map<lua_State*, SDK::USwitchBrick*> BrickByState;  // lua_State* -> brick key, for API lookups
luabridge::LuaRef* g_ApiTable; // shared read-only API, backing every brick's __index
luabridge::LuaRef* g_SandboxGlobals = nullptr;

void CreateEnableHooks();//Forward dec
void DisableDestroyHooks();//Forward dec

Function<void(SDK::FBrickChatMessage* This, SDK::EChatMessageType Type, SDK::ABrickPlayerController* Controller)> FBrickChatMessageConstructor("48 89 5C 24 08 57 48 83 EC 20 88");

void SendUserError(const std::string& errorMessage)
{
    auto PC = GetBrickPlayerController();
    SDK::FBrickChatMessage Message;
    FBrickChatMessageConstructor(&Message, SDK::EChatMessageType::Message, PC);
    std::wstring ErrorWString = to_wstring(errorMessage);
    SDK::FString ErrorString(ErrorWString.c_str());
    Message.TextOption = SDK::UKismetTextLibrary::Conv_StringToText(ErrorString);
    Message.Player.PlayerName = UC::FString(L"Lua");
    PC->ClientReceiveChatMessage(Message);
}

namespace InputChannel_RE
{
    struct FBrickEditorObjectID
    {
        unsigned __int16 ID;
    };

    template<typename T>
    struct __declspec(align(4)) TBrickEditorObjectPtr
    {
        SDK::TWeakObjectPtr<T> Ptr;
        FBrickEditorObjectID ID;
    };

    /* 201744 */
    struct FBrickEditorObjectPtr
    {
        TBrickEditorObjectPtr<SDK::UBrickEditorObject> Ptr;
    };

    float GetInputChannelValue(int Index)
    {
        if (ActiveBrick)
        {
            SDK::FVehicleInputChannel InputChannel = ActiveBrick->InputChannel;
            if (Index == -1) return InputChannel.Value;
            if (InputChannel.SourceBricks.Num() < Index) return 0.0f;
            {
                SDK::FBrickEditorObjectPtr Raw = InputChannel.SourceBricks[Index];
                FBrickEditorObjectPtr* Pointer = reinterpret_cast<FBrickEditorObjectPtr*>(&Raw);
                SDK::UBrickEditorObject* Obj = Pointer->Ptr.Ptr.Get();
                if (!Obj) return 0.0f;

                if (Obj->IsA(SDK::UMathBrick::StaticClass()))
                {
                    return reinterpret_cast<SDK::UMathBrick*>(Obj)->OutputChannel.CurrentValue;
                }
                if (Obj->IsA(SDK::USensorBrickBase::StaticClass()))
                {
                    return reinterpret_cast<SDK::USensorBrickBase*>(Obj)->OutputChannel.CurrentValue;
                }
                return 0.0f;
            }
        }
        return 0.0f;
    }
}

void SetOutputChannelValue(float Value)
{
    if (!ActiveBrick) return;
    ActiveBrick->SetOutputChannelValue(ActiveBrick->OutputChannel, Value);
}

void LoadLuaLibraries(lua_State* L)
{
    // 1. Enable base functions (gives you print, assert, type, etc.)
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);

    // 2. Enable math library (gives you math.sin, math.floor, etc.)
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);

    // 3. Enable string library (gives you string.find, string.format, etc.)
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);

    // 4. Enable table library (gives you table.insert, table.sort, etc.)
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);

    for (const char* name : { "load", "loadstring", "dofile", "loadfile", "require", "collectgarbage" })
    {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
}

void LuaRuntime::Initialize()
{
    L = luaL_newstate();
    LoadLuaLibraries(L);

    // Build a flat table combining your API functions with a whitelist of
    // real stdlib globals — this becomes every brick's __index fallback.
    lua_newtable(L); // sandbox globals table, sits at top of stack

    lua_pushcfunction(L, [](lua_State* L) -> int {
        lua_pushnumber(L, InputChannel_RE::GetInputChannelValue((int)luaL_checkinteger(L, 1)));
        return 1;
        });
    lua_setfield(L, -2, "GetInputChannel");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        SetOutputChannelValue((float)luaL_checknumber(L, 1));
        return 0;
        });
    lua_setfield(L, -2, "SetOutputChannel");

    // Whitelist specific real stdlib globals, including the REAL print
    const char* allowedGlobals[] = {
        "print", "tostring", "tonumber", "pairs", "ipairs",
        "type", "assert", "error", "pcall", "select", "unpack"
    };
    for (const char* name : allowedGlobals)
    {
        lua_getglobal(L, name);
        lua_setfield(L, -2, name);
    }

    // Whitelist whole safe library tables
    const char* allowedLibs[] = { "math", "string", "table" };
    for (const char* name : allowedLibs)
    {
        lua_getglobal(L, name);
        lua_setfield(L, -2, name);
    }

    g_SandboxGlobals = new luabridge::LuaRef(luabridge::LuaRef::fromStack(L, -1));
    lua_pop(L, 1);

    // Now lock down the REAL global table so nothing escapes through it directly
    for (const char* name : { "load", "loadstring", "dofile", "loadfile", "require", "collectgarbage", "BR" })
    {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }

    CreateEnableHooks();
}

void LuaRuntime::Shutdown()
{
    DisableDestroyHooks();
    delete g_SandboxGlobals;
    g_SandboxGlobals = nullptr;
    LuaBricks.clear();
    BrickByState.clear();
    if (L) { lua_close(L); L = nullptr; }
}

static int Lua_BlockGlobalWrite(lua_State* L)
{
    const char* key = lua_tostring(L, 2);
    if (key && strcmp(key, "Tick") == 0)
    {
        lua_rawset(L, 1); // bypass the metamethod, write directly into the env table
        return 0;
    }
    return luaL_error(L, "Global variables are not allowed ('%s'). Use the storage API instead.", key ? key : "?");
}

static void InstructionLimitHook(lua_State* L, lua_Debug*)
{
    luaL_error(L, "Script exceeded instruction limit (possible infinite loop)");
}

void PrintWholeString(std::wstring str)
{
    for (wchar_t c : str) std::cout << (int)c << " ";
        std::cout << "\n";
}

void AddLuaBrickToRuntime(SDK::USwitchBrick* Brick, SDK::ABrickVehicle* Vehicle)
{
    std::wstring ScriptID = Brick->SwitchName.ToWString();
    if (ScriptID.empty() || ScriptID.find(L'=') == std::string::npos) return;

    std::string source = "";
    for (SDK::UBrick* Brick : Vehicle->GetBricks())
    {
        if (Brick->IsA(SDK::UTextBrick::StaticClass()))
        {
            auto TextBrick = reinterpret_cast<SDK::UTextBrick*>(Brick);
            std::wstring TextBrickString = TextBrick->Text.ToWString();
            if (TextBrickString.empty() || TextBrickString.find(L'\n') == std::string::npos) return;
            std::wstring TextBrickScriptID = TextBrickString.substr(0, TextBrickString.find(L'\n')-1);//Remove carrige terminator
            if (ScriptID == TextBrickScriptID)
            {
                std::wcout << TextBrickString.substr(TextBrickString.find('\n')+1) << std::endl;
                source = TextBrick->Text.ToString().substr(TextBrickString.find('\n')+1);
            }
        }
    }

    if (source.empty()) SendUserError("Failed to find text brick with lua code!");

    lua_State* co = lua_newthread(L);
    lua_sethook(co, InstructionLimitHook, LUA_MASKCOUNT, LUA_MAX_INSTRUCTIONS_PER_TICK);
    luabridge::LuaRef threadRef(L, luabridge::LuaRef::fromStack(L, -1)); // capture the thread as a LuaRef
    lua_pop(L, 1); // LuaRef holds its own ref now, safe to pop L's stack copy

    // Private env table
    luabridge::LuaRef envRef = luabridge::newTable(co);

    // Metatable: __index -> shared API, __newindex -> reject writes
    lua_newtable(co); // metatable
    g_SandboxGlobals->push(co);      // was: g_ApiTable->push(co)
    lua_setfield(co, -2, "__index");
    lua_pushcclosure(co, Lua_BlockGlobalWrite, 0);
    lua_setfield(co, -2, "__newindex");
    envRef.push(co);
    lua_pushvalue(co, -2);      // metatable
    lua_setmetatable(co, -2);
    lua_pop(co, 2);             // pop metatable copy + envRef push

    if (luaL_loadstring(co, source.c_str()) != LUA_OK)
    {
        SendUserError(lua_tostring(co, -1));
        lua_pop(co, 1);
        return;
    }
    envRef.push(co);
    lua_setupvalue(co, -2, 1); // _ENV upvalue = private env table

    LuaBrick brick{ co, threadRef, envRef, Brick };
    auto [it, inserted] = LuaBricks.emplace(Brick, std::move(brick));
    BrickByState[co] = Brick; // now maps to the stable key, not a fragile pointer
    // ...
    if (lua_pcall(co, 0, 0, 0) != LUA_OK)
    {
        SendUserError(lua_tostring(co, -1));
        lua_pop(co, 1);
        it->second.HasError = true;
    }
    
}

void SetupVehicleLua(SDK::ABrickVehicle* Vehicle)
{
	// Setup Lua environment for the vehicle, load scripts, etc.
    for (SDK::UBrick* SwitchBrick : Vehicle->GetBricks()) // adjust to actual accessor
    {
        if (!IsLuaBrick(SwitchBrick)) continue;
        AddLuaBrickToRuntime(reinterpret_cast<SDK::USwitchBrick*>(SwitchBrick), Vehicle);
    }
}

void TickLuaBrick(LuaBrick& brick, float DeltaTime)
{
    if (brick.HasError) return;
    auto PC = GetBrickPlayerController();
    if (!PC || PC->PlayerVehicle != brick.Brick->GetVehicle()) return;

    luabridge::LuaRef tickFn = brick.EnvRef["Tick"];
    if (!tickFn.isFunction()) return;

    lua_sethook(brick.Coroutine, InstructionLimitHook, LUA_MASKCOUNT, LUA_MAX_INSTRUCTIONS_PER_TICK);
    ActiveBrick = brick.Brick;
    auto result = tickFn(DeltaTime);
    if (result.error())
    {
        SendUserError(result.message());
        brick.HasError = true;
    }
}

Hook<void(SDK::USwitchBrick*, float)> SwitchBrick_TickBrickHook("80 B9 ?? 01 00 00 ?? 74 ?? B2 ?? E9 ?? ?? ?? ?? C3",
[](SDK::USwitchBrick* This, float DeltaTime) -> void
{
    auto it = LuaBricks.find(This);
    if (it != LuaBricks.end())
    {
        TickLuaBrick(it->second, DeltaTime);
        return;
    }

    SwitchBrick_TickBrickHook.CallOriginalFunction(This, DeltaTime);
});

Hook<void(SDK::UPlayerInputComponent*, SDK::ABrickVehicle*)> OnPlayerVehicleChangedHook("40 53 48 83 EC ?? F6 81 ?? 00 00 00 ?? 48 8B D9 74 ?? E8 ?? ?? ?? ?? 48 8B CB",
[](SDK::UPlayerInputComponent* This, SDK::ABrickVehicle* Vehicle) -> void
{
    OnPlayerVehicleChangedHook.CallOriginalFunction(This, Vehicle);
    if (GetBrickPlayerController() && GetBrickPlayerController()->PlayerVehicle && Vehicle == GetBrickPlayerController()->PlayerVehicle)
    {
        for (auto& [brickPtr, brick] : LuaBricks)
        {
            BrickByState.erase(brick.Coroutine);
        }
        LuaBricks.clear();

        if (Vehicle)
        {
            SetupVehicleLua(Vehicle);
        }
    }
});

void CreateEnableHooks()
{
    SwitchBrick_TickBrickHook.Create();
    SwitchBrick_TickBrickHook.Enable();

    OnPlayerVehicleChangedHook.Create();
    OnPlayerVehicleChangedHook.Enable();
}

void DisableDestroyHooks()
{
    SwitchBrick_TickBrickHook.Disable();
    SwitchBrick_TickBrickHook.Destroy();

    OnPlayerVehicleChangedHook.Disable();
    OnPlayerVehicleChangedHook.Destroy();
}

/*
* How do we know when or when not to execute Lua code?
* Runtime - How do we handle errors?
* What happens when something breaks? Do we crash the game? Do we log it? Do we try to recover?
* How to report errors to the user? Do we have a console? Do we have a log file? Do we have a UI for errors?
*/