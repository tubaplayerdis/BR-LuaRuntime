#pragma once

#define LUA_MAX_INSTRUCTIONS_PER_TICK 10000
#include <string>

namespace LuaRuntime
{
	void Initialize();
	void Shutdown();

	void RaiseLuaException(std::string const& message);
}
