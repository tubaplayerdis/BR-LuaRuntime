local MathLib = require("MathLib")

function Tick(Delta)
	local X = GetInChannelVal(0)
	local Z = GetInChannelVal(1)
	print("X" .. X)
	print("Z" .. Z)
	SetOutChannelVal(Delta * MathLib.PI)
end

function Interact(Value)
	print(Value)
end