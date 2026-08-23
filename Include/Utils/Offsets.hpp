/*----------------------------------------------------------------------------*/
/*                                                                            */
/*    Copyright (c) Aaron Wilk 2025, All rights reserved.                     */
/*                                                                            */
/*    Module:     Offsets.hpp										          */
/*    Author:     Aaron Wilk                                                  */
/*    Created:    14 July 2025                                                */
/*                                                                            */
/*    Revisions:  V0.1                                                        */
/*                                                                            */
/*----------------------------------------------------------------------------*/


#pragma once
#include <windows.h>
#include <libloaderapi.h>

//Version of Brick Rigs the SDK is mapped to.

#define MAPPED_SDK_VERSION_RELEASE 1
#define MAPPED_SDK_VERSION_MAJOR 10
#define MAPPED_SDK_VERSION_MINOR 7

/*
* This file is used to store various information regarding non-sdk functions/hooks so that when an update rolls around it is easier to update the information
*
* Prefixes:
* F_ - Function offset
* V_ - VTable index
* H_ - Hook offset
* G_ - Global variable offset
*
* Suffixes:
* _S = Signature
* _M = Mask
*
* Hooks that use sigs/masks have suffixes as to not interfere with the existing naming conventions regarding singletons for the hooks.
*/

//Custom Assertion macro. takes an expression and a const char*. Aborts the game on expression failed.
#define ASSERT(exp, errmsg) \
    do { \
        if (!(exp)) { \
            char buf[512]; \
            snprintf(buf, sizeof(buf), \
                "Assertion failed!\n\nExpression: %s\nMessage: %s\nFile: %s\nLine: %d", \
                #exp, errmsg, __FILE__, __LINE__); \
            MessageBoxA(GetConsoleWindow(), buf, "BR-SDK Assertion FAILED", MB_OK); \
            std::abort(); \
        } \
    } while (0); \

#define BASE (unsigned long long )GetModuleHandle(NULL) //Add this to offsets. It is the module base of Brick Rigs and look like 0x14 in IDA