function Tick(Delta)
	local X = GetInputChannel(0)
	local Z = GetInputChannel(1)
	print("X" .. X)
	print("Z" .. Z)
	SetOutputChannel(Delta)
end