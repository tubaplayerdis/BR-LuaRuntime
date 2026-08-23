/*----------------------------------------------------------------------------*/
/*                                                                            */
/*    Copyright (c) Aaron Wilk 2025, All rights reserved.                     */
/*                                                                            */
/*    Module:     Utilities.hpp				                                  */
/*    Author:     Aaron Wilk                                                  */
/*    Created:    7 July 2025                                                 */
/*                                                                            */
/*    Revisions:  V0.1                                                        */
/*                                                                            */
/*----------------------------------------------------------------------------*/

#pragma once

#include "UtilitiesImplementation.hpp"

//BR-SDK utility macros.

/// Gets the UClass from the sdk class. Will load blueprint classes as necessary
/// @param cls Class of the new object. Not the UClass. Ex: SDK::UBrickBorder
/// @return A pointer to the UClass
#define GetUClass(cls) GetClassInternal<cls>(#cls)

/// Spawns a new UObject using internal UE systems. Use when creating UObjects. Loads the class if not loaded already
/// @param cls Class of the new object. Not the UClass. Ex: SDK::UBrickBorder
/// @param out A pointer to the outer object the new object should be created with
/// @return A pointer to the new object
#define SpawnObject(cls, out) SpawnObjectInternal<cls>(out, #cls)