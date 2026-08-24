#pragma once
#include <BR-SDK.hpp>

namespace BrickAPI
{
    void SetActiveBrick(SDK::USwitchBrick* Brick);

    float GetInputChannelValue(int Index);
    void SetInputChannelValue(int Index, float Value);
    void SetOutputChannelValue(float Value);
    float GetOutputChannelValue();
}
