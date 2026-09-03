--=MathLib
-- Remove the comment prefex (--) above when using in BR!
local M = {}
function M.clamp(x, lo, hi)
    if x < lo then return lo end
    if x > hi then return hi end
    return x
end
M.PI = 3.14159
return M
