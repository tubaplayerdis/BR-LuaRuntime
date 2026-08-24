#pragma once

#define LUA_MAX_INSTRUCTIONS_PER_TICK 10000

namespace LuaRuntime
{
	void Initialize();
	void Shutdown();
}