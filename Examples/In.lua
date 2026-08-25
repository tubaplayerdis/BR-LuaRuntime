--In Define these globally?
local In = {}
    function In.Get(Index)
        return GetInChannelVal(Index)
    end
    function In.Set(Index, Value)
        SetInChannelVal(Index, Value)
    end
return In