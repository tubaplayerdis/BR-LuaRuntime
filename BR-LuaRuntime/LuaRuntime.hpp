#pragma once
#include <Lua/lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include <BR-SDK.hpp>

struct LuaBrick
{
	lua_State* Coroutine;                // raw pointer for fast lookup/dispatch
	luabridge::LuaRef ThreadRef;         // keeps the coroutine alive (was: int threadRef + luaL_ref/unref)
	luabridge::LuaRef EnvRef;            // this brick's private _ENV table
	SDK::USwitchBrick* Brick;
	bool HasError = false;
};

namespace LuaRuntime
{
	void Initialize();
	void Shutdown();
}