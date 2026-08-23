/*----------------------------------------------------------------------------*/
/*                                                                            */
/*    Copyright (c) Aaron Wilk 2025, All rights reserved.                     */
/*                                                                            */
/*    Module:     UtilitiesImplementation.hpp				      */
/*    Author:     Aaron Wilk                                                  */
/*    Created:    14 July 2025                                                */
/*                                                                            */
/*    Revisions:  V0.1                                                        */
/*                                                                            */
/*----------------------------------------------------------------------------*/

#pragma once
#include "../SDK/Basic.hpp"
#include "../SDK/BrickRigs_classes.hpp"
#include "../SDK/UMG_classes.hpp"
#include "../../Include/Utils/Offsets.hpp"
#include "../../Include/Utils/GameFunctions.hpp"
#include "../../Include/SDK/AssetRegistry_classes.hpp"
#include "../../Include/Utils/UnrealContainers.hpp"

template<typename T>
SDK::UClass* GetClassInternal(const char* clsobjname)
{
	auto RegistryInterface = SDK::UAssetRegistryHelpers::GetAssetRegistry();
	SDK::IAssetRegistry* Registry = static_cast<SDK::IAssetRegistry*>(RegistryInterface.GetInterfaceRef());
	SDK::TArray<SDK::FAssetData> data = SDK::TArray<SDK::FAssetData>();
	std::wstring clsobjname_w = UtfN::StringToWString(std::string(clsobjname));
	Registry->GetAssetsByClass(SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(clsobjname_w.c_str())), &data, true);
	return SDK::UAssetRegistryHelpers::GetClass(data[0]);
}

//Class safe version of SDK::UGameplayStatics::SpawnObject. Use the SpawnObject() macro instead of this function.
template<typename T>
T* SpawnObjectInternal(SDK::UObject* outerobj, const char* objclsname)
{
	SDK::UClass* objcls = GetClassInternal<T>(objclsname);
	return static_cast<T*>(SDK::UGameplayStatics::SpawnObject(objcls, outerobj));
}

/**
 * This was meant to be macro but didnt fit with the SDK design
 */
template<typename T>
T* SpawnActorInternal(SDK::FTransform transform, SDK::AActor* outeract, const char* objclsname)
{
	SDK::UClass* objcls = GetClassInternal<T>(objclsname);
	SDK::ESpawnActorCollisionHandlingMethod method = SDK::ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SDK::AActor* act = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(SDK::UWorld::GetWorld(), objcls, transform, method, outeract);
	SDK::UGameplayStatics::FinishSpawningActor(act, transform);
	return static_cast<T*>(act);
}