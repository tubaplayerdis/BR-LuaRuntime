--Out Define these globally?
local Out = {}
    function Out.Get()
        return GetOutChannelVal()
    end
    function Out.Set(Value)
        SetOutChannelVal(Value)
    end
return Out