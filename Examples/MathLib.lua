--=MathLib
-- Include the above at the top of your TextBrick text, without the comment prefix "--"

--This is an example of a non-execution module. It is shared by execution modules and cannot be used as an execution module.

local M = {}

function M.clamp(x, lo, hi)
    if x < lo then return lo end
    if x > hi then return hi end
    return x
end

M.PI = 3.14159

return M