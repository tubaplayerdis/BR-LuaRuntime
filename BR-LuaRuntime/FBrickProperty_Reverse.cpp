#include <BR-SDK.hpp>
DWORD_PTR GetStaticAddressFromVA(PVOID va);

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

Hook<void(SDK::USwitchBrick* This, FBrickPropertyReflection* Params)> UTextBrick_ReflectPropertiesHook("40 55 53 56 57 41 55 41 56 41 57 48 8D 6C 24 ?? 48 81 EC ?? ?? ?? ?? 4C 89 A4 24 ?? ?? ?? ??",
[](SDK::USwitchBrick* This, FBrickPropertyReflection* Params) -> void
{
    UTextBrick_ReflectPropertiesHook.CallOriginalFunction(This, Params);
    auto Props = Params->BrickProperties;
    std::cout << Props.Num() << " " << Props.Max() << std::endl;
    static SDK::FString FTextBrickPropertyStringType = SDK::FString(L"FTextBrickProperty");
    static SDK::FName FTextBrickPropertyNameType = SDK::UKismetStringLibrary::Conv_StringToName(FTextBrickPropertyStringType);
    static SDK::FString TextStringType = SDK::FString(L"Text");
    static SDK::FName TextNameType = SDK::UKismetStringLibrary::Conv_StringToName(TextStringType);
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
                if (TextBrickProperty->PropertyName == TextNameType)
                {
                    //TextBrickProperty->VTable->PrintAddresses();
                    //TextBrickProperty->bAllowMultiLine = true;
                    TextBrickProperty->MaxTextLength = 32767;
                }
            }
        }
        std::cout << PropertyInstance.FullPropertyName.ToString() << std::endl;
    }
});