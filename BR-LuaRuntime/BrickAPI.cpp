#include "BrickAPI.hpp"

#include "LuaRuntime.hpp"

SDK::USwitchBrick* ActiveBrick = nullptr;
std::unordered_map<std::string, SDK::USwitchBrick*> SwitchBricks;

Function<void(SDK::USwitchBrick* This, bool bImmediate)> USwitchBrick_UpdateSwitchValue("48 89 5C 24 08 57 48 83 EC 20 48 8B F9 84 D2 0F 84 D6");

namespace //Reverse Engineered
{
    struct FBrickEditorObjectID
    {
        unsigned __int16 ID;
    };

    template<typename T>
    struct __declspec(align(4)) TBrickEditorObjectPtr
    {
        SDK::TWeakObjectPtr<T> Ptr;
        FBrickEditorObjectID ID;
    };

    /* 201744 */
    struct FBrickEditorObjectPtr
    {
        TBrickEditorObjectPtr<SDK::UBrickEditorObject> Ptr;
    };
}

void BrickAPI::SetActiveBrick(SDK::USwitchBrick* Brick)
{
    ActiveBrick = Brick;
}

SDK::USwitchBrick* BrickAPI::GetActiveBrick()
{
    return ActiveBrick;
}

float BrickAPI::GetInputChannelValue(int Index)
{
    if (ActiveBrick)
    {
        SDK::FVehicleInputChannel InputChannel = ActiveBrick->InputChannel;
        if (Index == -1) return InputChannel.Value;
        if (InputChannel.SourceBricks.Num() <= Index)
        {
            LuaRuntime::RaiseLuaException("Input Channel Index out of bounds: " + std::to_string(Index));
            return 0.0f;
        }

        SDK::FBrickEditorObjectPtr Raw = InputChannel.SourceBricks[Index];
        FBrickEditorObjectPtr* Pointer = reinterpret_cast<FBrickEditorObjectPtr*>(&Raw);
        SDK::UBrickEditorObject* Obj = Pointer->Ptr.Ptr.Get();
        if (!Obj)
        {
            LuaRuntime::RaiseLuaException("UBrickEditorObject NULL at index: " + std::to_string(Index));
            return 0.0;
        }


        if (Obj->IsA(SDK::UMathBrick::StaticClass()))
        {
            return reinterpret_cast<SDK::UMathBrick*>(Obj)->OutputChannel.CurrentValue;
        }
        if (Obj->IsA(SDK::USensorBrickBase::StaticClass()))
        {
            return reinterpret_cast<SDK::USensorBrickBase*>(Obj)->OutputChannel.CurrentValue;
        }
        LuaRuntime::RaiseLuaException("UBrickEditorObject NOT UBrickSensorBase OR UMathBrick. Type: " + Obj->GetName());
        return 0.0f;

    }
    LuaRuntime::RaiseLuaException("ActiveBrick NULL");
    return 0.0f;
}

void BrickAPI::SetInputChannelValue(int Index, float Value)
{
    if (ActiveBrick)
    {
        SDK::FVehicleInputChannel InputChannel = ActiveBrick->InputChannel;
        if (Index == -1)
        {
            ActiveBrick->InputChannel.Value = Value;
            USwitchBrick_UpdateSwitchValue(ActiveBrick, true);
            return;
        }
        if (InputChannel.SourceBricks.Num() <= Index)
        {
            LuaRuntime::RaiseLuaException("Input Channel Index out of bounds: " + std::to_string(Index));
            return;
        }

        SDK::FBrickEditorObjectPtr Raw = InputChannel.SourceBricks[Index];
        FBrickEditorObjectPtr* Pointer = reinterpret_cast<FBrickEditorObjectPtr*>(&Raw);
        SDK::UBrickEditorObject* Obj = Pointer->Ptr.Ptr.Get();
        if (!Obj)
        {
            LuaRuntime::RaiseLuaException("UBrickEditorObject NULL at index: " + std::to_string(Index));
            return;
        }

        if (Obj->IsA(SDK::UMathBrick::StaticClass()))
        {
            reinterpret_cast<SDK::UMathBrick*>(Obj)->OutputChannel.CurrentValue = Value;
        }
        if (Obj->IsA(SDK::USensorBrickBase::StaticClass()))
        {
            reinterpret_cast<SDK::USensorBrickBase*>(Obj)->OutputChannel.CurrentValue = Value;
        }

        LuaRuntime::RaiseLuaException("UBrickEditorObject NOT UBrickSensorBase OR UMathBrick. Type: " + Obj->GetName());
    }
    LuaRuntime::RaiseLuaException("ActiveBrick NULL");
}

void BrickAPI::SetOutputChannelValue(float Value)
{
    if (!ActiveBrick) return;
    ActiveBrick->SetOutputChannelValue(ActiveBrick->OutputChannel, Value);
}

float BrickAPI::GetOutputChannelValue()
{
    if (!ActiveBrick) return 0.0f;
    return ActiveBrick->OutputChannel.CurrentValue;
}

void BrickAPI::ClearSwitchBrickRegistry()
{
    SwitchBricks.clear();
}

void BrickAPI::RegisterSwitchBrick(SDK::USwitchBrick* Brick)
{
    if (!Brick) return;
    std::string Name = Brick->SwitchName.ToString();
    SwitchBricks.insert_or_assign(Name, Brick);
}

float BrickAPI::GetOutputChannelValueNamed(const char* Name)
{
    std::string BrickName(Name);
    if (SwitchBricks.find(BrickName) == SwitchBricks.end())
    {
        LuaRuntime::RaiseLuaException("Could not find a SwitchBrick of name: " + BrickName);
        return 0.0f;
    }
    return SwitchBricks[BrickName]->OutputChannel.CurrentValue;
}

void BrickAPI::SetOutputChannelValueNamed(const char* Name, float Value)
{
    std::string BrickName(Name);
    if (SwitchBricks.find(BrickName) == SwitchBricks.end())
    {
        LuaRuntime::RaiseLuaException("Could not find a SwitchBrick of name: " + BrickName);
        return;
    }
    SDK::USwitchBrick* Brick = SwitchBricks[BrickName];
    Brick->SetOutputChannelValue(Brick->OutputChannel, Value);
}
