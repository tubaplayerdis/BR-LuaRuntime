--=Store
local Store = {}
    function Store:new(SwitchName)
        setmetatable({}, self)
        self.SwitchName = SwitchName
        return self
    end

    function Store:Set(Value)
        SetOutChannelValNamed(self.SwitchName, Value)
    end

    function Store:Get(Value)
        return GetOutChannelValNamed(self.SwitchName)
    end
return Store