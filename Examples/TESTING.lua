--=TESTING
-- Include the above at the top of your TextBrick text, without the comment prefix "--"
-- Remove the comments to use this code!
LDT = Store("LastDeltaTime")

--This function HAS to be implemented globally in Execution modules
function Tick(Delta)
	local X = In.Get(0) --Reads the bound brick on index 0's output channel value
	local Z = In.Get(1) --Reads the bound brick on index 1's output channel value
	local XZ = In.Get(-1) --Reads this bricks input channel
	print("X" .. X)
	print("Z" .. Z)
	Out.Set(Delta) --Sets this bricks output channel value
	LDT.Set(Delta) --Looks for a switch brick named "LastDeltaTime" and sets its output channel to Delta
	print(LDT.Get()) --Looks for s switch brick named "LastDeltaTime" and reads its output channel value
end