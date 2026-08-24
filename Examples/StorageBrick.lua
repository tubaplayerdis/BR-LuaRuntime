--=StorageBrick
-- Include the above at the top of your TextBrick text, without the comment prefix "--"

--This is an API abstraction over the standard BR-Lua functions.

local StorageBrick = {}

function StorageBrick:new(SwitchName)
    local instance = { SwitchName = SwitchName }
    setmetatable(instance, self)
    return instance
end

function StorageBrick:StoreValue(Value)
    SetInChannelValNamed(self.SwitchName, Value)
end

function StorageBrick:GetValue(Value)
    return GetInChannelValNamed(self.SwitchName)
end

return StorageBrick