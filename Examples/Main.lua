--=Main
LDT = Store("LastDeltaTime")

function Tick(Delta)
    local X = In.Get(0)
    local Z = In.Get(1)
    local XZ = In.Get(-1)
    print("X" .. X)
    print("Z" .. Z)
    Out.Set(Delta)
    LDT:Set(Delta)
    print(LDT:Get())
end