local StorageBrick = {}

function StorageBrick:new(SwitchName)
    local instance = { SwitchName = SwitchName }
    setmetatable(instance, self)
    return instance
end

--TODO: Implement these functions. Also might be able to provide an OOP abstraction over the pure functional API for Input and Output Channels

function StorageBrick:StoreValue(Value)
    SetInChannelVal_Named(self.SwitchName, Value)
end

function StorageBrick:GetValue(Value)
    return GetInChannelVal_Named(self.SwitchName)
end

return StorageBrick