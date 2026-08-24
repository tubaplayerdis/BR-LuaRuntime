--=TESTING
-- Include the above at the top of your TextBrick text, without the comment prefix "--"

--This function HAS to be implemented globally in Execution modules
function Tick(Delta)
	local X = GetInChannelVal(0) --Reads the bound brick on index 0 output channel value
	local Z = GetInChannelVal(1) --Reads the bound brick on index 1 output channel value
	local XZ = GetInChannelVal(-1) --Reads this bricks input channel
	print("X" .. X)
	print("Z" .. Z)
	SetOutChannelVal(Delta) --Sets this bricks output channel value
	SetOutChannelValNamed("LastDeltaTime", Delta) --Looks for a switch brick named "LastDeltaTime" and sets its output channel to Delta
	local LastDeltaTime = GetOutChannelValNamed("LastDeltaTime") --Looks for s switch brick named "LastDeltaTime" and reads its output channel value
	print(LastDeltaTime)
end

--This function HAS to be implemented globally in Execution modules
function Interact(Value)
	print(Value)
end