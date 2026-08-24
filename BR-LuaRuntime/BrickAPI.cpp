#include "BrickAPI.hpp"

SDK::USwitchBrick* ActiveBrick = nullptr;

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

float BrickAPI::GetInputChannelValue(int Index)
{
    if (ActiveBrick)
    {
        SDK::FVehicleInputChannel InputChannel = ActiveBrick->InputChannel;
        if (Index == -1) return InputChannel.Value;
        if (InputChannel.SourceBricks.Num() < Index) return 0.0f;
        {
            SDK::FBrickEditorObjectPtr Raw = InputChannel.SourceBricks[Index];
            FBrickEditorObjectPtr* Pointer = reinterpret_cast<FBrickEditorObjectPtr*>(&Raw);
            SDK::UBrickEditorObject* Obj = Pointer->Ptr.Ptr.Get();
            if (!Obj) return 0.0f;

            if (Obj->IsA(SDK::UMathBrick::StaticClass()))
            {
                return reinterpret_cast<SDK::UMathBrick*>(Obj)->OutputChannel.CurrentValue;
            }
            if (Obj->IsA(SDK::USensorBrickBase::StaticClass()))
            {
                return reinterpret_cast<SDK::USensorBrickBase*>(Obj)->OutputChannel.CurrentValue;
            }
            return 0.0f;
        }
    }
    return 0.0f;
}

void BrickAPI::SetInputChannelValue(int Index, float Value)
{
    if (ActiveBrick)
    {
        SDK::FVehicleInputChannel InputChannel = ActiveBrick->InputChannel;
        if (Index == -1)
        {
            InputChannel.Value = Value;
            return;
        }
        if (InputChannel.SourceBricks.Num() < Index) return;
        {
            SDK::FBrickEditorObjectPtr Raw = InputChannel.SourceBricks[Index];
            FBrickEditorObjectPtr* Pointer = reinterpret_cast<FBrickEditorObjectPtr*>(&Raw);
            SDK::UBrickEditorObject* Obj = Pointer->Ptr.Ptr.Get();
            if (!Obj) return;

            if (Obj->IsA(SDK::UMathBrick::StaticClass()))
            {
                reinterpret_cast<SDK::UMathBrick*>(Obj)->OutputChannel.CurrentValue = Value;
            }
            if (Obj->IsA(SDK::USensorBrickBase::StaticClass()))
            {
                reinterpret_cast<SDK::USensorBrickBase*>(Obj)->OutputChannel.CurrentValue = Value;
            }
            return;
        }
    }
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
