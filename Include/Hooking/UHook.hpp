#pragma once
#include "Basic.hpp"
#include "CoreUObject_classes.hpp"
#include "../Utils/GameFunctions.hpp"

enum ELogVerbosity : UC::int8
{
    NoLogging     = 0x0,
    Fatal         = 0x1,
    Error         = 0x2,
    Warning       = 0x3,
    Display       = 0x4,
    Log           = 0x5,
    Verbose       = 0x6,
    VeryVerbose   = 0x7,
    All           = 0x7,
    NumVerbosity  = 0x8,
    VerbosityMask = 0xF,
    SetColor      = 0x40,
    BreakOnLog    = 0x80,
};

// Kismet bytecode opcodes relevant to parameter stepping.
// Confirm these against your target's EExprToken enum.
enum EExprToken_Frame : UC::uint8
{
    EX_EndFunctionParms_ = 0x16,
    EX_Default_           = 0x5A,
};

struct __declspec(align(8)) FOutputDevice
{
    void*       vTable;
    bool        bSuppressEventTag;
    bool        bAutoEmitLineTerminator;
    UC::int8    Pad_0[0x6];
};
static_assert(sizeof(FOutputDevice) == 0x10, "FOutputDevice size mismatch");

struct FOutParmRec
{
    SDK::FProperty*     Property;
    UC::uint8*          PropAddr;
    FOutParmRec*        NextOutParm;
};

struct FParamSlot
{
    static void DestroyValue(SDK::FProperty* Function, void* Data)
    {
        CallVTableFunction<void, void*>(0xF0, Function, Data);
    }

    SDK::FProperty*      Property = nullptr;
    std::vector<uint8_t> Storage;

    void* Data() { return Storage.data(); }
    const void* Data() const { return Storage.data(); }

    ~FParamSlot()
    {
        // Non-POD properties (FString/TArray/struct with constructor) need their
        // destructor run or heap-backed members leak.
        if (Property)
            DestroyValue(Property, Data());
    }

    // Move-only: DestroyValue must run exactly once per constructed value.
    FParamSlot() = default;
    FParamSlot(const FParamSlot&) = delete;
    FParamSlot& operator=(const FParamSlot&) = delete;
    FParamSlot(FParamSlot&& Other) noexcept
        : Property(Other.Property), Storage(std::move(Other.Storage))
    {
        Other.Property = nullptr;
    }
    FParamSlot& operator=(FParamSlot&& Other) noexcept
    {
        if (this != &Other)
        {
            if (Property)
                DestroyValue(Property, Data());
            Property = Other.Property;
            Storage = std::move(Other.Storage);
            Other.Property = nullptr;
        }
        return *this;
    }

    template<typename T>
    T& As() { return *reinterpret_cast<T*>(Data()); }

    bool IsBool() const
    {
        return Property && Property->ClassPrivate->Name.ToString() == "BoolProperty";
    }
};

struct __declspec(align(8)) FFrame : FOutputDevice
{
    SDK::UFunction*             Node;
    SDK::UObject*               Object;
    UC::uint8*                  Code;
    UC::uint8*                  Locals;
    SDK::FProperty*             MostRecentProperty;
    UC::uint8*                  MostRecentPropertyAddress;
    UC::int8                    FlowStack[0x30]; //Actually a TArray
    FFrame*                     PreviousFrame;
    FOutParmRec*                OutParms;
    SDK::FField*                PropertyChainForCompiledIn;
    SDK::UFunction*             CurrentNativeFunction;
    bool                        bArrayContextFailed;
    UC::int8                    Pad_2[0x7];

    SDK::FString* GetStackTrace(SDK::FString* result);
    static void KismetExecutionMessage(const wchar_t* Message, ELogVerbosity Verbosity, SDK::FName WarningId);
    void Serialize(const wchar_t* V, ELogVerbosity Verbosity, const SDK::FName* Category);
    void Step(SDK::UObject* Context, void* Z_Param__Result);
    void StepExplicitProperty(void* Result, SDK::FProperty* Property);

    // --- Added helpers ---------------------------------------------------------

    // P_FINISH equivalent: consumes the EX_EndFunctionParms token.
    // Call once after reading every CPF_Parm property.
    void EndOfParms()
    {
        if (Code && *Code == EX_EndFunctionParms_)
            ++Code;
    }

    // Generic StepCompiledIn<T> equivalent. Prefer StepExplicitProperty (via this)
    // over raw Step() when you already know the FProperty, since it correctly
    // resolves out-parm addresses instead of evaluating a fresh expression.
    template<typename T>
    T StepCompiledIn(SDK::FProperty* ExpectedProperty)
    {
        T Value{};

        if (Code && *Code == EX_Default_)
        {
            ++Code;
            if (ExpectedProperty)
            {
                ExpectedProperty->CopyCompleteValueToScriptVM(
                    &Value,
                    ExpectedProperty->ContainerPtrToValuePtr<void>(Locals));
            }
        }
        else if (ExpectedProperty)
        {
            StepExplicitProperty(&Value, ExpectedProperty);
        }
        else
        {
            Step(Object, &Value);
        }

        return Value;
    }

    // Reads every non-return CPF_Parm property off the stack in declared order,
    // sized generically via Property->ElementSize. Also consumes EX_EndFunctionParms.
    // No compile-time type knowledge required — safe to use without an SDK for
    // the target game, since UFunction/FProperty are engine types.
    std::vector<FParamSlot> ReadAllParams()
    {
        std::vector<FParamSlot> Params;

        for (SDK::FProperty* Prop = Node->GetPropertyLink(); Prop; Prop = Prop->GetPropertyLinkNext())
        {
            if (!Prop->HasAnyPropertyFlags(SDK::EPropertyFlags::Parm) || Prop->HasAnyPropertyFlags(SDK::EPropertyFlags::ReturnParm))
                continue;

            FParamSlot Slot;
            Slot.Property = Prop;
            Slot.Storage.resize(Prop->ElementSize, 0);
            Prop->InitializeValue(Slot.Data());

            StepExplicitProperty(Slot.Data(), Prop);

            Params.push_back(std::move(Slot));
        }

        EndOfParms();
        return Params;
    }

    // Locate a parameter by name without positional indexing.
    SDK::FProperty* FindParamByName(SDK::FName Name)
    {
        for (SDK::FProperty* Prop = Node->GetPropertyLink(); Prop; Prop = Prop->GetPropertyLinkNext())
            if (Prop->HasAnyPropertyFlags(SDK::EPropertyFlags::Parm) && Prop->Name == Name)
                return Prop;
        return nullptr;
    }

    SDK::FProperty* GetReturnProperty()
    {
        for (SDK::FProperty* Prop = Node->GetPropertyLink(); Prop; Prop = Prop->GetPropertyLinkNext())
            if (Prop->HasAnyPropertyFlags(SDK::EPropertyFlags::ReturnParm))
                return Prop;
        return nullptr;
    }

    // Writes into the caller-provided result slot (Z_Param__Result in a DECLARE_FUNCTION body).
    template<typename T>
    void SetReturn(void* Z_Param__Result, const T& Value)
    {
        if (SDK::FProperty* RetProp = GetReturnProperty())
            RetProp->CopyCompleteValueFromScriptVM(Z_Param__Result, (void*)&Value); // was CopyCompleteValue
    }

    // --- Out-parameter write-back ----------------------------------------------
    // StepExplicitProperty only reads a *copy* into your scratch buffer. To write
    // a new value back to the caller's actual variable (a "ref"/"out" param),
    // you need the real address from the OutParms chain.

    void* FindOutParmAddress(SDK::FProperty* Property)
    {
        for (FOutParmRec* Rec = OutParms; Rec; Rec = Rec->NextOutParm)
            if (Rec->Property == Property)
                return Rec->PropAddr;
        return nullptr;
    }

    template<typename T>
    bool WriteOutParam(SDK::FProperty* Property, const T& Value)
    {
        void* Addr = FindOutParmAddress(Property);
        if (!Addr) return false;
        Property->CopyCompleteValueFromScriptVM(Addr, (void*)&Value); // was CopyCompleteValue
        return true;
    }
};
static_assert(sizeof(FFrame) == 0x98, "FFrame size mismatch");

class UHook
{
public:

    using ExecFunc = void (*)(SDK::UObject* Context, FFrame* TheStack, void* Result);

    UHook(ExecFunc NewFunction);
    UHook(SDK::UFunction* Function, ExecFunc NewFunction);
    UHook(SDK::UClass* Class, std::string Name, ExecFunc NewFunction);

    bool Set(SDK::UFunction* Function);
    bool Set(SDK::UClass* Class, std::string Name);

    bool Set();
    bool Enabled();
    bool Disabled()
    {
        return !Enabled();
    }

    void Enable();
    void Disable();
    void Toggle(bool bToggle)
    {
        bToggle ? Enable() : Disable();
    }

    void CallOriginal(SDK::UObject* Context, FFrame* TheStack, void* Result);

    static UC::int8 GetNumParams(SDK::UFunction* Function)
    {
        return GetMember<UC::int8>(Function, 0xB4);
    }

    static UC::int16 GetParamSize(SDK::UFunction* Function)
    {
        return GetMember<UC::int16>(Function, 0xB6);
    }

    static UC::int16 GetReturnValOffset(SDK::UFunction* Function)
    {
        return GetMember<UC::int16>(Function, 0xB8);
    }

private:
    bool HadFlag;
    SDK::UFunction::FNativeFuncPtr Original;
    SDK::UFunction* UEFunction;
    ExecFunc NewFunction;
};
