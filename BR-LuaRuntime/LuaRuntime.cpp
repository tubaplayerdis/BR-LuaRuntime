#include "LuaRuntime.hpp"
#include <Lua/lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include <BR-SDK.hpp>

std::wstring to_wstring(const std::string& str);
DWORD_PTR GetStaticAddressFromVA(PVOID va);

SDK::ABrickPlayerController* GetBrickPlayerController()
{
	SDK::APlayerController* PlayerController = SDK::UGameplayStatics::GetPlayerController(SDK::UWorld::GetWorld(), 0);
	if (PlayerController)
	{
		return static_cast<SDK::ABrickPlayerController*>(PlayerController);
	}
	return nullptr;
}

bool IsLuaBrick(SDK::UBrick* Brick)
{
    auto SI = Brick->GetStaticInfo();
    bool IsLuaBrick = Brick && SI->IsA(SDK::USwitchBrickStaticInfo::StaticClass()) && SI->GetName() == "Default__BP_LuaBrick_C";
    std::cout << "Lua Brick!\n";
    return IsLuaBrick;
}

lua_State* L = nullptr;

struct LuaBrick
{
    lua_State* Coroutine;                // raw pointer for fast lookup/dispatch
    luabridge::LuaRef ThreadRef;         // keeps the coroutine alive (was: int threadRef + luaL_ref/unref)
    luabridge::LuaRef EnvRef;            // this brick's private _ENV table
    SDK::USwitchBrick* Brick;
    bool HasError = false;
};
SDK::USwitchBrick* ActiveBrick = nullptr;

std::unordered_map<SDK::USwitchBrick*, LuaBrick> LuaBricks;       // owns the contexts, node-stable
std::unordered_map<lua_State*, SDK::USwitchBrick*> BrickByState;  // lua_State* -> brick key, for API lookups
luabridge::LuaRef* g_ApiTable; // shared read-only API, backing every brick's __index
luabridge::LuaRef* g_SandboxGlobals = nullptr;

void CreateEnableHooks();//Forward dec
void DisableDestroyHooks();//Forward dec

Function<void(SDK::FBrickChatMessage* This, SDK::EChatMessageType Type, SDK::ABrickPlayerController* Controller)> FBrickChatMessageConstructor("48 89 5C 24 08 57 48 83 EC 20 88");

void SendUserError(const std::string& errorMessage)
{
    auto PC = GetBrickPlayerController();
    SDK::FBrickChatMessage Message;
    FBrickChatMessageConstructor(&Message, SDK::EChatMessageType::Message, PC);
    std::wstring ErrorWString = to_wstring(errorMessage);
    SDK::FString ErrorString(ErrorWString.c_str());
    Message.TextOption = SDK::UKismetTextLibrary::Conv_StringToText(ErrorString);
    Message.Player.PlayerName = UC::FString(L"Lua");
    PC->ClientReceiveChatMessage(Message);
}

float GetInputChannelValue(int Index)
{
	if (ActiveBrick)
	{
		SDK::FVehicleInputChannel InputChannel = ActiveBrick->InputChannel;
        if (InputChannel.SourceBricks.Num() > Index) return 0.0f;
        return InputChannel.Value;
	}
	return 0.0f;
}

void SetOutputChannelValue(float Value)
{
    if (!ActiveBrick) return;
    ActiveBrick->SetOutputChannelValue(ActiveBrick->OutputChannel, Value);
}

void LoadLuaLibraries(lua_State* L)
{
    // 1. Enable base functions (gives you print, assert, type, etc.)
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);

    // 2. Enable math library (gives you math.sin, math.floor, etc.)
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);

    // 3. Enable string library (gives you string.find, string.format, etc.)
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);

    // 4. Enable table library (gives you table.insert, table.sort, etc.)
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);

    for (const char* name : { "load", "loadstring", "dofile", "loadfile", "require", "collectgarbage" })
    {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
}

void LuaRuntime::Initialize()
{
    L = luaL_newstate();
    LoadLuaLibraries(L);

    // Build a flat table combining your API functions with a whitelist of
    // real stdlib globals — this becomes every brick's __index fallback.
    lua_newtable(L); // sandbox globals table, sits at top of stack

    lua_pushcfunction(L, [](lua_State* L) -> int {
        lua_pushnumber(L, GetInputChannelValue((int)luaL_checkinteger(L, 1)));
        return 1;
        });
    lua_setfield(L, -2, "GetInputChannel");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        SetOutputChannelValue((float)luaL_checknumber(L, 1));
        return 0;
        });
    lua_setfield(L, -2, "SetOutputChannel");

    // Whitelist specific real stdlib globals, including the REAL print
    const char* allowedGlobals[] = {
        "print", "tostring", "tonumber", "pairs", "ipairs",
        "type", "assert", "error", "pcall", "select", "unpack"
    };
    for (const char* name : allowedGlobals)
    {
        lua_getglobal(L, name);
        lua_setfield(L, -2, name);
    }

    // Whitelist whole safe library tables
    const char* allowedLibs[] = { "math", "string", "table" };
    for (const char* name : allowedLibs)
    {
        lua_getglobal(L, name);
        lua_setfield(L, -2, name);
    }

    g_SandboxGlobals = new luabridge::LuaRef(luabridge::LuaRef::fromStack(L, -1));
    lua_pop(L, 1);

    // Now lock down the REAL global table so nothing escapes through it directly
    for (const char* name : { "load", "loadstring", "dofile", "loadfile", "require", "collectgarbage", "BR" })
    {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }

    CreateEnableHooks();
}

void LuaRuntime::Shutdown()
{
    DisableDestroyHooks();
    delete g_SandboxGlobals;
    g_SandboxGlobals = nullptr;
    LuaBricks.clear();
    BrickByState.clear();
    if (L) { lua_close(L); L = nullptr; }
}

static int Lua_BlockGlobalWrite(lua_State* L)
{
    const char* key = lua_tostring(L, 2);
    if (key && strcmp(key, "Tick") == 0)
    {
        lua_rawset(L, 1); // bypass the metamethod, write directly into the env table
        return 0;
    }
    return luaL_error(L, "Global variables are not allowed ('%s'). Use the storage API instead.", key ? key : "?");
}

static void InstructionLimitHook(lua_State* L, lua_Debug*)
{
    luaL_error(L, "Script exceeded instruction limit (possible infinite loop)");
}

void AddLuaBrickToRuntime(SDK::USwitchBrick* Brick, SDK::ABrickVehicle* Vehicle)
{
    std::string IDOfScript = Brick->SwitchName.ToString();
    std::string source = "";
    for (SDK::UBrick* Brick : Vehicle->GetBricks())
    {
        if (Brick->IsA(SDK::UTextBrick::StaticClass()) && std::to_string((int)reinterpret_cast<UC::int16>(Brick->GetEditorObjectID().Pad_0)) == IDOfScript)
        {
            auto TextBrick = reinterpret_cast<SDK::UTextBrick*>(Brick);
            source = TextBrick->Text.ToString();
        }
    }

    lua_State* co = lua_newthread(L);
    lua_sethook(co, InstructionLimitHook, LUA_MASKCOUNT, 1000);
    luabridge::LuaRef threadRef(L, luabridge::LuaRef::fromStack(L, -1)); // capture the thread as a LuaRef
    lua_pop(L, 1); // LuaRef holds its own ref now, safe to pop L's stack copy

    // Private env table
    luabridge::LuaRef envRef = luabridge::newTable(co);

    // Metatable: __index -> shared API, __newindex -> reject writes
    lua_newtable(co); // metatable
    g_SandboxGlobals->push(co);      // was: g_ApiTable->push(co)
    lua_setfield(co, -2, "__index");
    lua_pushcclosure(co, Lua_BlockGlobalWrite, 0);
    lua_setfield(co, -2, "__newindex");
    envRef.push(co);
    lua_pushvalue(co, -2);      // metatable
    lua_setmetatable(co, -2);
    lua_pop(co, 2);             // pop metatable copy + envRef push

    if (luaL_loadstring(co, source.c_str()) != LUA_OK)
    {
        SendUserError(lua_tostring(co, -1));
        lua_pop(co, 1);
        return;
    }
    envRef.push(co);
    lua_setupvalue(co, -2, 1); // _ENV upvalue = private env table

    LuaBrick brick{ co, threadRef, envRef, Brick };
    auto [it, inserted] = LuaBricks.emplace(Brick, std::move(brick));
    BrickByState[co] = Brick; // now maps to the stable key, not a fragile pointer
    // ...
    if (lua_pcall(co, 0, 0, 0) != LUA_OK)
    {
        SendUserError(lua_tostring(co, -1));
        lua_pop(co, 1);
        it->second.HasError = true;
    }
    
}

void SetupVehicleLua(SDK::ABrickVehicle* Vehicle)
{
	// Setup Lua environment for the vehicle, load scripts, etc.
    for (SDK::UBrick* SwitchBrick : Vehicle->GetBricks()) // adjust to actual accessor
    {
        if (!IsLuaBrick(SwitchBrick)) continue;
        AddLuaBrickToRuntime(reinterpret_cast<SDK::USwitchBrick*>(SwitchBrick), Vehicle);
    }
}

void TickLuaBrick(LuaBrick& brick, float DeltaTime)
{
    if (brick.HasError) return;

    luabridge::LuaRef tickFn = brick.EnvRef["Tick"];
    if (!tickFn.isFunction()) return;

    ActiveBrick = brick.Brick;
    auto result = tickFn(DeltaTime);
    if (result.error())
    {
        SendUserError(result.message());
        brick.HasError = true;
    }
}

Hook<void(SDK::USwitchBrick*, float)> SwitchBrick_TickBrickHook("80 B9 ?? 01 00 00 ?? 74 ?? B2 ?? E9 ?? ?? ?? ?? C3",
[](SDK::USwitchBrick* This, float DeltaTime) -> void
{
    auto it = LuaBricks.find(This);
    if (it != LuaBricks.end())
    {
        TickLuaBrick(it->second, DeltaTime);
        return;
    }

    SwitchBrick_TickBrickHook.CallOriginalFunction(This, DeltaTime);
});

Hook<void(SDK::UPlayerInputComponent*, SDK::ABrickVehicle*)> OnPlayerVehicleChangedHook("40 53 48 83 EC ?? F6 81 ?? 00 00 00 ?? 48 8B D9 74 ?? E8 ?? ?? ?? ?? 48 8B CB",
[](SDK::UPlayerInputComponent* This, SDK::ABrickVehicle* Vehicle) -> void
{
    OnPlayerVehicleChangedHook.CallOriginalFunction(This, Vehicle);
    if (GetBrickPlayerController() && GetBrickPlayerController()->PlayerVehicle && Vehicle == GetBrickPlayerController()->PlayerVehicle)
    {
        for (auto& [brickPtr, brick] : LuaBricks)
        {
            BrickByState.erase(brick.Coroutine);
        }
        LuaBricks.clear();

        if (Vehicle)
        {
            SetupVehicleLua(Vehicle);
        }
    }
});

template<typename T>
struct TSharedRef
{
    T* Object;
    UC::int8 SharedReferenceCount[0x8];
};

template<typename T>
struct TSharedPtr
{
    T* Object;
    UC::int8 SharedReferenceCount[0x8];
};

template<typename T>
struct TWeakPtr
{
    T* Object;
    UC::int8 WeakReferenceCount[0x8];
};

template<typename T>
struct TSharedFromThis
{
    TWeakPtr<T> WeakThis;
};

const struct FBrickPropertyContainer
{
    SDK::UObject *RootObject;
    SDK::TArray<void *> ContainerChain;
};

const struct FBrickEditorReferenceResolver
{
    SDK::TArray<SDK::UObject *> Objects;
};

struct FBrickProperty;
struct FBrickProperty_vtbl
{
    SDK::FName *(__fastcall *GetTypeName)(FBrickProperty *This, SDK::FName *result);
    SDK::FName *(__fastcall *GetValueTypeName)(FBrickProperty *This, SDK::FName *result);
    bool (__fastcall *IsOfTypeInternal)(FBrickProperty *This, const SDK::FName *);
    void (__fastcall *GetTypeHierarchyInternal)(FBrickProperty *This, SDK::TArray<SDK::FName> *);
    void (__fastcall *Destructor_FBrickProperty)(FBrickProperty *This);
    void (__fastcall *GetTypeHierarchy)(FBrickProperty *This, SDK::TArray<SDK::FName> *);
    bool (__fastcall *ComparePropertyValues)(FBrickProperty *This, const void *, const void *);
    bool (__fastcall *IsPropertyNull)(FBrickProperty *This);//Name guessed from the behavior of the function
    bool (__fastcall *FluppuSpecialSauce1)(FBrickProperty *This, void*, void*);
    bool (__fastcall *SerializeProperty)(FBrickProperty *This, void* FArchive_Ptr, const FBrickPropertyContainer* Container, UC::int8 Version, const FBrickEditorReferenceResolver*);
    bool (__fastcall *DoesObjectContainPropertyInternal)(FBrickProperty *This, const SDK::UObject *);
    bool (__fastcall *GetValueAsText)(FBrickProperty *This, const FBrickPropertyContainer *, SDK::FText *);
    bool (__fastcall *SetValueAsText)(FBrickProperty *This, const FBrickPropertyContainer *, const SDK::FText *);
    bool (__fastcall *IsUserText)(FBrickProperty *This);
    SDK::FString *(__fastcall *ExportProperty)(FBrickProperty *This, SDK::FString *result, const FBrickPropertyContainer *);
    bool (__fastcall *CanExportProperty)(FBrickProperty *This, const FBrickPropertyContainer *);
    bool (__fastcall *ImportProperty)(FBrickProperty *This, const FBrickPropertyContainer *, const wchar_t *);
    bool (__fastcall *CanImportProperty)(FBrickProperty *This, const FBrickPropertyContainer *, const wchar_t *);

    void PrintAddresses()
    {
        std::cout << std::hex << std::showbase;

        std::cout << "GetTypeName: "                  << GetStaticAddressFromVA(this->GetTypeName)                  << std::endl;
        std::cout << "GetValueTypeName: "              << GetStaticAddressFromVA(this->GetValueTypeName)             << std::endl;
        std::cout << "IsOfTypeInternal: "              << GetStaticAddressFromVA(this->IsOfTypeInternal)             << std::endl;
        std::cout << "GetTypeHierarchyInternal: "      << GetStaticAddressFromVA(this->GetTypeHierarchyInternal)     << std::endl;
        std::cout << "Destructor_FBrickProperty: "     << GetStaticAddressFromVA(this->Destructor_FBrickProperty)    << std::endl;
        std::cout << "GetTypeHierarchy: "              << GetStaticAddressFromVA(this->GetTypeHierarchy)             << std::endl;
        std::cout << "ComparePropertyValues: "         << GetStaticAddressFromVA(this->ComparePropertyValues)        << std::endl;
        std::cout << "IsPropertyNull: "                << GetStaticAddressFromVA(this->IsPropertyNull)    << std::endl;
        std::cout << "FluppiSauce1: "                  << GetStaticAddressFromVA(this->FluppuSpecialSauce1)    << std::endl;
        std::cout << "SerializeProperty: "             << GetStaticAddressFromVA(this->SerializeProperty)            << std::endl;
        std::cout << "DoesObjectContainPropertyInternal: " << GetStaticAddressFromVA(this->DoesObjectContainPropertyInternal) << std::endl;
        std::cout << "GetValueAsText: "                << GetStaticAddressFromVA(this->GetValueAsText)               << std::endl;
        std::cout << "SetValueAsText: "                << GetStaticAddressFromVA(this->SetValueAsText)               << std::endl;
        std::cout << "IsUserText: "                    << GetStaticAddressFromVA(this->IsUserText)                   << std::endl;
        std::cout << "ExportProperty: "                << GetStaticAddressFromVA(this->ExportProperty)               << std::endl;
        std::cout << "CanExportProperty: "             << GetStaticAddressFromVA(this->CanExportProperty)            << std::endl;
        std::cout << "ImportProperty: "                << GetStaticAddressFromVA(this->ImportProperty)               << std::endl;
        std::cout << "CanImportProperty: "             << GetStaticAddressFromVA(this->CanImportProperty)            << std::endl;

        std::cout << std::dec; // reset stream state if you print anything decimal afterward
    }
};

struct FBrickProperty
{
    FBrickProperty_vtbl* VTable;
    SDK::FProperty* Property;
    SDK::FName PropertyName;
};

struct __declspec(align(2)) FTextBrickProperty : FBrickProperty
{
  /*const*/ int MaxTextLength;
  /*const*/ bool bIsPassword;
  /*const*/ bool bAllowMultiLine;
  /*const*/ bool bIsUserText;
  UC::int8 Pad_0[0x9];
};
static_assert(sizeof(FTextBrickProperty) == 0x28);

struct FBrickPropertyCategory
{
    SDK::FText DisplayName;
};

struct FBrickPropertyInstance
{
    TSharedRef<FBrickProperty> BrickProperty;
    SDK::FString FullPropertyName;
    SDK::TArray<SDK::FStructProperty> ParentPropertyChain;
};
static_assert(sizeof(FBrickPropertyInstance) == 0x30);

const struct __declspec(align(8)) FBrickPropertyChangedEvent
{
    SDK::TWeakObjectPtr<SDK::ABasePlayerController> Player;
    SDK::FString FullPropertyName;
    SDK::TArray<SDK::FName> PropertyChain;
    int PropertyChainDepth;
    SDK::TArray<SDK::TWeakObjectPtr<SDK::UObject>> Objects;
    SDK::TWeakObjectPtr<SDK::UObject> ActiveObject;
    SDK::EValueChangedEventType EventType[1];
    bool bExternalChange;
    bool bUpdateAllProperties;
};

const struct __declspec(align(8)) FBrickPropertyEditInfo : FBrickPropertyInstance, TSharedFromThis<FBrickPropertyEditInfo>
{
    SDK::FText DisplayName;
    SDK::FText DescriptionText;
    SDK::TArray<SDK::TWeakObjectPtr<SDK::UObject>> ContainerObjects;
    bool bIsEnabled;
    bool bIsReadOnly;
    SDK::EBrickUIColorStyle ColorStyle[1];
    SDK::uint8 pad_0[1];
    int MaxComboBoxListItems;
    int MaxComboBoxItemsPerRow;
    SDK::uint8 pad_1[4];
    TSharedPtr<FBrickPropertyChangedEvent> PendingChangedEvent;
    SDK::TOptional<std::byte> OrientationOverride;
    SDK::uint8 pad_2[6];
};
static_assert(sizeof(FBrickPropertyEditInfo) == 0xA8);

struct FBrickPropertyReflection
{
    bool bIsSerializing;
    SDK::uint8 pad_0[7];
    SDK::TArray<SDK::TWeakObjectPtr<SDK::UObject>> ContainerObjects;
    SDK::FBrickPropertyReflectionFilter Filter;
    void* SomethingFluppiAdded;
    SDK::TArray<FBrickPropertyInstance> BrickProperties;
    SDK::TArray<SDK::TPair<TSharedRef<FBrickPropertyEditInfo>, int>> BrickPropertyEditInfos;
    SDK::TArray<FBrickPropertyCategory> Categories;
    int CurrentCategoryIndex;
    SDK::uint8 pad_1[4];
    SDK::TArray<SDK::FStructProperty*> ParentPropertyChain;
};
static_assert(sizeof(FBrickPropertyReflection) == 0x88);
static_assert(offsetof(FBrickPropertyReflection, BrickPropertyEditInfos) == 0x50);

Hook<void(SDK::USwitchBrick* This, FBrickPropertyReflection* Params)> USwitchBrick_ReflectPropertiesHook("40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 F8 48 81 EC 08 01 00 00 45",
[](SDK::USwitchBrick* This, FBrickPropertyReflection* Params) -> void
{
    USwitchBrick_ReflectPropertiesHook.CallOriginalFunction(This, Params);
    auto Props = Params->BrickProperties;
    std::cout << Props.Num() << " " << Props.Max() << std::endl;
    static SDK::FString FTextBrickPropertyStringType = SDK::FString(L"FTextBrickProperty");
    static SDK::FName FTextBrickPropertyNameType = SDK::UKismetStringLibrary::Conv_StringToName(FTextBrickPropertyStringType);
    static SDK::FString SwitchNameStringType = SDK::FString(L"SwitchName");
    static SDK::FName SwitchNameNameType = SDK::UKismetStringLibrary::Conv_StringToName(SwitchNameStringType);
    for (int i = 0; i < Props.Num(); i++)
    {
        FBrickPropertyInstance PropertyInstance = Props[i];
        if (PropertyInstance.BrickProperty.Object)
        {
            auto BrickProperty = PropertyInstance.BrickProperty.Object;
            if (BrickProperty->VTable->IsOfTypeInternal(BrickProperty, &FTextBrickPropertyNameType))
            {
                std::cout << "Is FTextBrickProperty" << std::endl;
                auto TextBrickProperty = reinterpret_cast<FTextBrickProperty*>(BrickProperty);
                if (TextBrickProperty->PropertyName == SwitchNameNameType)
                {
                    //TextBrickProperty->VTable->PrintAddresses();
                    //TextBrickProperty->bAllowMultiLine = true;
                    //TextBrickProperty->MaxTextLength = 32767;
                }
            }
        }
        std::cout << PropertyInstance.FullPropertyName.ToString() << std::endl;
    }
});

Hook<bool(FTextBrickProperty *This, void* FArchive_Ptr, const FBrickPropertyContainer* Container, UC::int8 Version, const FBrickEditorReferenceResolver* Res)> FTextBrickProperty_SerializePropertyHook("48 8B C4 53 48 83 EC 40 48 89 68 08 48 8B DA 48 89 70 10 48",
    [](FTextBrickProperty *This, void* FArchive_Ptr, const FBrickPropertyContainer* Container, UC::int8 Version, const FBrickEditorReferenceResolver* Res) -> bool
    {
        static SDK::FString SwitchNameStringType1 = SDK::FString(L"SwitchName");
        static SDK::FName SwitchNameNameType1 = SDK::UKismetStringLibrary::Conv_StringToName(SwitchNameStringType1);
        std::cout << "BRUH" << This->PropertyName.GetRawString() << std::endl;
        if (This->PropertyName == SwitchNameNameType1)
        {
            This->bAllowMultiLine = true;
            This->MaxTextLength = 32767;
            std::cout << "Activated" << std::endl;
        }
        return FTextBrickProperty_SerializePropertyHook.CallOriginalFunction(This, FArchive_Ptr, Container, Version, Res);
    });

void CreateEnableHooks()
{
    SwitchBrick_TickBrickHook.Create();
    SwitchBrick_TickBrickHook.Enable();

    OnPlayerVehicleChangedHook.Create();
    OnPlayerVehicleChangedHook.Enable();

    //USwitchBrick_ReflectPropertiesHook.Create();
    //USwitchBrick_ReflectPropertiesHook.Enable();

    //FTextBrickProperty_SerializePropertyHook.Create();
    //FTextBrickProperty_SerializePropertyHook.Enable();
}

void DisableDestroyHooks()
{
    SwitchBrick_TickBrickHook.Disable();
    SwitchBrick_TickBrickHook.Destroy();

    OnPlayerVehicleChangedHook.Disable();
    OnPlayerVehicleChangedHook.Destroy();

    //USwitchBrick_ReflectPropertiesHook.Disable();
    //USwitchBrick_ReflectPropertiesHook.Destroy();

    //FTextBrickProperty_SerializePropertyHook.Disable();
    //FTextBrickProperty_SerializePropertyHook.Destroy();
}

/*
* How do we know when or when not to execute Lua code?
* Runtime - How do we handle errors?
* What happens when something breaks? Do we crash the game? Do we log it? Do we try to recover?
* How to report errors to the user? Do we have a console? Do we have a log file? Do we have a UI for errors?
*/