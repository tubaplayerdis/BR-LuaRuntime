#pragma once
#include <BR-SDK.hpp>

namespace BrickAPI
{
    void SetActiveBrick(SDK::USwitchBrick* Brick);
    SDK::USwitchBrick* GetActiveBrick();

    float GetInputChannelValue(int Index);
    void SetInputChannelValue(int Index, float Value);
    void SetOutputChannelValue(float Value);
    float GetOutputChannelValue();

    void ClearSwitchBrickRegistry();
    void RegisterSwitchBrick(SDK::USwitchBrick* Brick);
    float GetOutputChannelValueNamed(const char* Name);
    void SetOutputChannelValueNamed(const char* Name, float Value);
}
