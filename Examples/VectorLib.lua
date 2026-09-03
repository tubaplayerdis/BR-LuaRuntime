--VectorLib
-- Remove the comment prefex (--) above when using!
local V = {}
    function V.new()
        local self = setmetatable({}, V)
        self.x = 0
        self.y = 0
        self.z = 0
        return self
    end
    function V.update()
        self.x = In.Get(0);
        self.y = In.Get(1);
        self.z = In.Get(2);
    end
    function V.printDelta(Other)
        print("dx " .. self.x)
        print("dy " .. self.y)
        print("dz " .. self.z)
    end
return V
