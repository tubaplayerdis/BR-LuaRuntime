/*----------------------------------------------------------------------------*/
/*                                                                            */
/*    Copyright (c) Aaron Wilk 2025, All rights reserved.                     */
/*                                                                            */
/*    Module:     GameFunctions.hpp					      */
/*    Author:     Aaron Wilk                                                  */
/*    Created:    14 July 2025                                                */
/*                                                                            */
/*    Revisions:  V0.1                                                        */
/*                                                                            */
/*----------------------------------------------------------------------------*/

#pragma once
#include <stdexcept>
#include "../Hooking/Signature.hpp"
#include <windows.h>
#include <libloaderapi.h>
#include <cstdlib>
#include <utility>
#include <psapi.h>

/// Call an internal game function using its address.
/// @param addr Address of the function.
/// @param sig Signature of the function
/// @param ... Arguments to pass
#define CALL_GAME_FUNCTION(addr, sig, ...) ( (reinterpret_cast<sig>(addr))(__VA_ARGS__) )

/// Call an internal game function using its address.
/// @tparam TRet Return type of the function
/// @tparam TArgs Argument types of the function
/// @param addr Address of the function.
/// @param args Arguments to pass
/// @return TRet
template<typename TRet, typename... TArgs>
TRet CallGameFunction(unsigned long long addr, TArgs... args)
{
    using FunctionFn = TRet(__fastcall*)(TArgs...);
    FunctionFn OnFunction = reinterpret_cast<FunctionFn>(addr);
    return OnFunction(std::forward<TArgs>(args)...);
}

/// Call an internal game function using its signature. WILL NOT WORK IF A HOOK IS HOOKING THE DESIRED FUNCTION.
/// @note THIS WILL NOT WORK IF A HOOK HAS BEEN REGISTERED FOR THIS FUNCTION BEFOREHAND. Use the CallOriginal() function on the hook.
/// @tparam TRet Return type of the function
/// @tparam TArgs Argument types of the function
/// @param signature Signature of the function.
/// @param args Arguments to pass
/// @return TRet
/// @throws runtime_error The signature as a runtime error when the signature function is not found.
template<typename TRet, typename... TArgs>
TRet CallGameFunction(const char* signature, TArgs... args)
{
    unsigned long long addr = Signature(signature).GetPtr();
    if (addr == 0) throw std::runtime_error(signature);
    return CallGameFunction<TRet, TArgs...>(addr, args...);
}

/// Call an internal game function using its signature. WILL NOT WORK IF A HOOK IS HOOKING THE DESIRED FUNCTION.
/// @note THIS WILL NOT WORK IF A HOOK HAS BEEN REGISTERED FOR THIS FUNCTION BEFOREHAND. Use the CallOriginal() function on the hook.
/// @tparam TRet Return type of the function
/// @tparam TArgs Argument types of the function
/// @param signature Signature of the function.
/// @param args Arguments to pass
/// @return TRet
/// @throws runtime_error The signature as a runtime error when the signature function is not found.
template<typename TRet, typename... TArgs>
TRet CallGameFunction(Signature& signature, TArgs... args)
{
    unsigned long long addr = signature.GetPtr();
    if (addr == 0) throw std::runtime_error(signature.GetSig());
    return CallGameFunction<TRet, TArgs...>(addr, args...);
}

/// Call an internal game function based on its index in a vtable. If calling inside a hook macro surround with parentheses.
/// @tparam TRet Return type of the function
/// @tparam TArgs Argument types of the function
/// @param index Index of the function. Do not predivide by 8
/// @param object Valid pointer to an object of that vtable
/// @param args Arguments to pass
/// @return TRet
template<typename TRet, typename... TArgs>
TRet CallVTableFunction(int index, void* object, TArgs... args)
{
    using FunctionFn = TRet(__fastcall*)(void*, TArgs...);
    void** vtable = *reinterpret_cast<void***>(object);
    FunctionFn FunctionFunc = reinterpret_cast<FunctionFn>(vtable[index]);
    return FunctionFunc(object, std::forward<TArgs>(args)...);
}

/// Gets the member of an object given an offset
/// @tparam T member type
/// @param base Address of the object.
/// @param offset offset of the member
/// @return T
template<typename T>
T& GetMember(void* base, std::size_t offset)
{
    return *reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(base) + offset);
}

/// Sets the member of an object given an offset
/// @tparam T member type
/// @param base Address of the object.
/// @param offset offset of the member
/// @param value value to set the member
template<typename T>
void SetMember(void* base, std::size_t offset, const T& value)
{
    *reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(base) + offset) = value;
}

/// Casts a pointer
/// @tparam T pointer type
/// @param obj pointer object to cast
/// @return T*
template<typename T>
T* Cast(void* obj)
{
    return static_cast<T*>(obj);
}

/// Find a function address.
/// @param pattern Pattern of the function. Must adhere to format: \x00
/// @param mask Mask of the function. Use x confirmed and ? for wildcards
/// @return A pointer to a function of that pattern and mask
unsigned long long FindPatternF(const char* pattern, const char* mask);
