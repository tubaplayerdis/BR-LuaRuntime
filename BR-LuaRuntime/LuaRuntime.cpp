#include "LuaRuntime.hpp"
#include <Lua/lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include <BR-SDK.hpp>
#include "BrickAPI.hpp"
#include "Helpers.hpp"
#include "VehicleAPI.hpp"

struct LuaBrick
{
    lua_State* Coroutine;                // raw pointer for fast lookup/dispatch
    luabridge::LuaRef ThreadRef;         // keeps the coroutine alive (was: int threadRef + luaL_ref/unref)
    luabridge::LuaRef EnvRef;            // this brick's private _ENV table
    SDK::USwitchBrick* Brick;
    bool HasError = false;
};

lua_State* L = nullptr;
std::unordered_map<SDK::USwitchBrick*, LuaBrick> LuaBricks;       // owns the contexts, node-stable
std::unordered_map<lua_State*, SDK::USwitchBrick*> BrickByState;  // lua_State* -> brick key, for API lookups
luabridge::LuaRef* g_ApiTable; // shared read-only API, backing every brick's __index
luabridge::LuaRef* g_SandboxGlobals = nullptr;
std::unordered_map<std::string, luabridge::LuaRef> g_LoadedModules;

void CreateEnableHooks();//Forward dec
void DisableDestroyHooks();//Forward dec

static int Lua_Require(lua_State* callerL)
{
    const char* name = luaL_checkstring(callerL, 1);
    auto cached = g_LoadedModules.find(name);
    if (cached != g_LoadedModules.end())
    {
        cached->second.push(callerL);
        return 1;
    }

    SDK::ABrickVehicle* Vehicle = Helpers::GetBrickPlayerController()->PlayerVehicle; // or pass vehicle context through some other means
    std::string source = Helpers::FindModuleSource(Vehicle, name);
    if (source.empty())
        return luaL_error(callerL, "Module '%s' not found", name);

    //Modules get global state and can theroretically interact with eachother
    lua_State* modco = lua_newthread(callerL);
    luabridge::LuaRef modThreadRef(callerL, luabridge::LuaRef::fromStack(callerL, -1));
    lua_pop(callerL, 1);

    luabridge::LuaRef modEnv = luabridge::newTable(modco);
    lua_newtable(modco);
    g_SandboxGlobals->push(modco);
    lua_setfield(modco, -2, "__index");
    modEnv.push(modco);
    lua_pushvalue(modco, -2);
    lua_setmetatable(modco, -2);
    lua_pop(modco, 2);

    if (luaL_loadstring(modco, source.c_str()) != LUA_OK)
        return luaL_error(callerL, "Module '%s' failed to compile: %s", name, lua_tostring(modco, -1));

    modEnv.push(modco);
    lua_setupvalue(modco, -2, 1);

    if (lua_pcall(modco, 0, 1, 0) != LUA_OK)
        return luaL_error(callerL, "Module '%s' errored: %s", name, lua_tostring(modco, -1));

    lua_xmove(modco, L, 1); // moves to the real global L
    luabridge::LuaRef result = luabridge::LuaRef::fromStack(L, -1);
    lua_pop(L, 1);
    g_LoadedModules.emplace(name, result);

    result.push(callerL);
    return 1;
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

    lua_pushcfunction(L, [](lua_State* L) -> int
    {
        lua_pushnumber(L, BrickAPI::GetInputChannelValue((int)luaL_checkinteger(L, 1)));
        return 1;
    });
    lua_setfield(L, -2, "GetInChannelVal");

    lua_pushcfunction(L, [](lua_State* L) -> int
    {
        BrickAPI::SetOutputChannelValue((float)luaL_checknumber(L, 1));
        return 0;
    });
    lua_setfield(L, -2, "SetOutChannelVal");

    lua_pushcfunction(L, [](lua_State* L) -> int
    {
        BrickAPI::SetInputChannelValue((int)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2));
        return 0;
    });
    lua_setfield(L, -2, "SetInChannelVal");

    lua_pushcfunction(L, [](lua_State* L) -> int
    {
        lua_pushnumber(L, BrickAPI::GetOutputChannelValue());
        return 1;
    });
    lua_setfield(L, -2, "GetOutChannelVal");

    lua_pushcfunction(L, [](lua_State* L) -> int
    {
        BrickAPI::SetInputChannelValueNamed(luaL_checklstring(L, 1, nullptr), luaL_checknumber(L, 2));
        return 0;
    });
    lua_setfield(L, -2, "SetInChannelValNamed");

    lua_pushcfunction(L, [](lua_State* L) -> int
    {
        lua_pushnumber(L, BrickAPI::GetInputChannelValueNamed(luaL_checklstring(L, 1, nullptr)));
        return 1;
    });
    lua_setfield(L, -2, "GetInChannelValNamed");

    lua_pushcfunction(L, Lua_Require);
    lua_setfield(L, -2, "require");

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

lua_State* LastCallingState = nullptr;

void SetLastCallingState(lua_State* L)
{
    LastCallingState = L;
}

void LuaRuntime::RaiseLuaException(std::string const& message)
{
    SDK::USwitchBrick* Active = BrickAPI::GetActiveBrick();
    std::wstring ErrorContext = L"";
    if (Active)
    {
        ErrorContext = Active->SwitchName.ToWString();
    }

    std::wcout << ErrorContext.c_str() << std::endl;
    Helpers::SendUserError(ErrorContext, message);
    
    if (LastCallingState == nullptr) return;
    luaL_error(LastCallingState, "%s", message.c_str());
}

static int Lua_ProtectApiNames(lua_State* L)
{
    // __newindex receives: 1 = table, 2 = key, 3 = value
    const char* key = lua_tostring(L, 2);

    if (key)
    {
        luabridge::LuaRef existing = (*g_SandboxGlobals)[key];
        if (existing.isFunction())
        {
            return luaL_error(L, "'%s' is a reserved API function and cannot be overwritten.", key);
        }
    }

    lua_rawset(L, 1); // bypass the metamethod, write directly into the brick's env table
    return 0;
}

static void InstructionLimitHook(lua_State* L, lua_Debug*)
{
    luaL_error(L, "Script exceeded instruction limit (possible infinite loop)");
}

void AddLuaBrickToRuntime(SDK::USwitchBrick* Brick, SDK::ABrickVehicle* Vehicle)
{
    std::string ScriptID = Brick->SwitchName.ToString();
    if (ScriptID.empty() || ScriptID.find('=') == std::string::npos) return;
    ScriptID = ScriptID.substr(ScriptID.find('=')+1);

    std::string source = Helpers::FindModuleSource(Vehicle, ScriptID);

    if (source.empty()) Helpers::SendUserError("Failed to find text brick with lua code!");

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
    lua_pushcclosure(co, Lua_ProtectApiNames, 0); // was: Lua_BlockGlobalWrite (or newly added if you'd removed __newindex entirely)
    lua_setfield(co, -2, "__newindex");
    envRef.push(co);
    lua_pushvalue(co, -2);      // metatable
    lua_setmetatable(co, -2);
    lua_pop(co, 2);             // pop metatable copy + envRef push

    if (luaL_loadstring(co, source.c_str()) != LUA_OK)
    {
        Helpers::SendUserError(lua_tostring(co, -1));
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
        Helpers::SendUserError(lua_tostring(co, -1));
        lua_pop(co, 1);
        it->second.HasError = true;
    }
    
}

void SetupVehicleLua(SDK::ABrickVehicle* Vehicle)
{
    //Remove old variables, scripts etc
    for (auto& [brickPtr, brick] : LuaBricks)
    {
        BrickByState.erase(brick.Coroutine);
    }
    LuaBricks.clear();
    g_LoadedModules.clear();
    BrickAPI::ClearSwitchBrickRegistry();

	// Setup Lua environment for the vehicle, load scripts, etc.
    for (SDK::UBrick* SwitchBrick : Vehicle->GetBricks()) // adjust to actual accessor
    {
        if (SwitchBrick->IsA(SDK::USwitchBrick::StaticClass()))
        {
            BrickAPI::RegisterSwitchBrick(reinterpret_cast<SDK::USwitchBrick*>(SwitchBrick));
        }
        if (!Helpers::IsLuaBrick(SwitchBrick)) continue;
        AddLuaBrickToRuntime(reinterpret_cast<SDK::USwitchBrick*>(SwitchBrick), Vehicle);
    }
}

void TickLuaBrick(LuaBrick& brick, float DeltaTime)
{
    if (brick.HasError) return;
    auto PC = Helpers::GetBrickPlayerController();
    if (!PC || PC->PlayerVehicle != brick.Brick->GetVehicle()) return;

    luabridge::LuaRef tickFn = brick.EnvRef["Tick"];
    if (!tickFn.isFunction()) return;

    lua_sethook(brick.Coroutine, InstructionLimitHook, LUA_MASKCOUNT, LUA_MAX_INSTRUCTIONS_PER_TICK);
    BrickAPI::SetActiveBrick(brick.Brick);
    SetLastCallingState(brick.Coroutine);
    auto result = tickFn(DeltaTime);
    if (result.error())
    {
        Helpers::SendUserError(result.message());
        brick.HasError = true;
    }
}

void InteractLuaBrick(LuaBrick& brick, UC::uint8 Value)
{
    if (brick.HasError) return;
    auto PC = Helpers::GetBrickPlayerController();
    if (!PC || PC->PlayerVehicle != brick.Brick->GetVehicle()) return;

    luabridge::LuaRef tickFn = brick.EnvRef["Interact"];
    if (!tickFn.isFunction()) return;

    lua_sethook(brick.Coroutine, InstructionLimitHook, LUA_MASKCOUNT, LUA_MAX_INSTRUCTIONS_PER_TICK);
    BrickAPI::SetActiveBrick(brick.Brick);
    SetLastCallingState(brick.Coroutine);
    auto result = tickFn(Value);
    if (result.error())
    {
        Helpers::SendUserError(result.message());
        brick.HasError = true;
    }
}

Hook<void(SDK::USwitchBrick*, float)> SwitchBrick_TickBrickHook("80 B9 ?? 01 00 00 ?? 74 ?? B2 ?? E9 ?? ?? ?? ?? C3",
[](SDK::USwitchBrick* This, float DeltaTime) -> void
{
    auto PC = Helpers::GetBrickPlayerController();
    auto Veh = This->GetVehicle();

    if (!VehicleAPI::Valid(PC, Veh))
    {
        SwitchBrick_TickBrickHook.CallOriginalFunction(This, DeltaTime);
        return;
    }

    if (VehicleAPI::SetActiveVehicle(Veh))
    {
        SetupVehicleLua(This->GetVehicle());
    }

    SwitchBrick_TickBrickHook.CallOriginalFunction(This, DeltaTime);

    if (This->IsBrickDamaged()) return;

    auto it = LuaBricks.find(This);
    if (it != LuaBricks.end())
    {
        TickLuaBrick(it->second, DeltaTime);
        return;
    }
});

Hook<void(SDK::ABrickPlayerController*, SDK::USwitchBrick*, int NewValue)> ABrickPlayerController_SetSwitchBrickValueHook("48 85 D2 0F 84 A0 00 00 00 48 89 5C",
    [](SDK::ABrickPlayerController* This, SDK::USwitchBrick* SwitchBrick, int NewValue) -> void
    {
        auto PC = Helpers::GetBrickPlayerController();
        auto Veh = SwitchBrick->GetVehicle();

        if (!VehicleAPI::Valid(PC, Veh))
        {
            ABrickPlayerController_SetSwitchBrickValueHook.CallOriginalFunction(This, SwitchBrick, NewValue);
            return;
        }

        if (VehicleAPI::SetActiveVehicle(Veh))
        {
            SetupVehicleLua(SwitchBrick->GetVehicle());
        }

        ABrickPlayerController_SetSwitchBrickValueHook.CallOriginalFunction(This, SwitchBrick, NewValue);

        auto it = LuaBricks.find(SwitchBrick);
        if (it != LuaBricks.end())
        {
            InteractLuaBrick(it->second, static_cast<UC::int8>(NewValue));
        }
    });

Hook<void(SDK::ABrickPlayerController*, SDK::FPlayerSpawnRequest*)> ABrickPlayerController_RestartAtHook("48 89 5C 24 08 48 89 74 24 10 55 57 41 56 48 8D 6C 24 B0",
    [](SDK::ABrickPlayerController* This, SDK::FPlayerSpawnRequest* SpawnRequest) -> void
    {
        if (This == Helpers::GetBrickPlayerController()) VehicleAPI::SetActiveVehicle(nullptr);//Invalidate to cause reload of lua
        ABrickPlayerController_RestartAtHook.CallOriginalFunction(This, SpawnRequest);
    });

void CreateEnableHooks()
{
    SwitchBrick_TickBrickHook.Create();
    SwitchBrick_TickBrickHook.Enable();

    ABrickPlayerController_SetSwitchBrickValueHook.Create();
    ABrickPlayerController_SetSwitchBrickValueHook.Enable();

    ABrickPlayerController_RestartAtHook.Create();
    ABrickPlayerController_RestartAtHook.Enable();
}

void DisableDestroyHooks()
{
    SwitchBrick_TickBrickHook.Disable();
    SwitchBrick_TickBrickHook.Destroy();

    ABrickPlayerController_SetSwitchBrickValueHook.Disable();
    ABrickPlayerController_SetSwitchBrickValueHook.Destroy();

    ABrickPlayerController_RestartAtHook.Disable();
    ABrickPlayerController_RestartAtHook.Destroy();
}