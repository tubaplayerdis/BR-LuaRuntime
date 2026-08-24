--=TESTING
-- Include the above at the top of your TextBrick text, without the comment prefix "--"

local MathLib = require("MathLib")

--This function HAS to be implemented globally in Execution modules
function Tick(Delta)
	local X = GetInChannelVal(0) --Reads the bound brick on index 0 output channel value
	local Z = GetInChannelVal(1) --Reads the bound brick on index 1 output channel value
	local XZ = GetInChannelVal(-1) --Reads this bricks input channel
	print("X" .. X)
	print("Z" .. Z)
	SetOutChannelVal(Delta * MathLib.PI) --Sets this bricks output channel value
	SetOutChannelValNamed("LastDeltaTime", Delta) --Looks for a switch brick named "LastDeltaTime" and sets its input channel to Delta
	local LastDeltaTime = GetOutChannelValNamed("LastDeltaTime") --Looks for s switch brick named "LastDeltaTime" and reads its input channel
	print(LastDeltaTime)
end

--This function HAS to be implemented globally in Execution modules
function Interact(Value)
	print(Value)
end