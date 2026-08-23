/*----------------------------------------------------------------------------*/
/*                                                                            */
/*    Copyright (c) Aaron Wilk 2026, All rights reserved.                     */
/*                                                                            */
/*    Module:     Signature.hpp		                                          */
/*    Author:     Aaron Wilk                                                  */
/*    Created:    27 May 2026                                                 */
/*                                                                            */
/*    Revisions:  V0.5                                                        */
/*                                                                            */
/*----------------------------------------------------------------------------*/

#pragma once
#include <iostream>
#include <string>
/**
    * @brief Signature class with caching functionality.
    *
    * @code Signature UBRICK_GETFUELLEVEL("40 53 48 83 EC ?? 48 8B 01 48 8B D9 FF 90 ?? ?? ?? ?? 48 8B C8 48 85 C0 75 ??");
*/
class Signature
{
    std::string Sig;
public:

    /**
     * @brief Create a signature
     * @note THIS WILL NOT WORK IF A HOOK HAS BEEN REGISTERED FOR THIS FUNCTION BEFOREHAND. Use the CallOriginal() function on the hook.
     * @param signature Signature of the function. Uses the format: "48 89 7C 24 ?? 41 56 48 83 EC ?? 48 8B FA 4C 8B F1 E8 ?? ?? ?? ??".
     */
    explicit Signature(const char* signature) noexcept;

    /**
     * @brief Creates an address only signature. For use by Hook class.
     * @param address The address of the function.
     */
    explicit Signature(std::uintptr_t address) noexcept;

    /**
     * @brief Resolves the signature to a function pointer.
     * @return Function pointer if successful.
     * @returns 0 if no function was found.
     */
    std::uintptr_t GetPtr() const;

    /**
     * @brief This does what you think it does.
     */
    std::string GetSig() const;

};

template <typename>
class Function;
//maybe add hooking functionality?
template <typename Ret, typename... Args>
class Function<Ret(Args...)> : public Signature
{
public:
    Function(uintptr_t ptr) noexcept : Signature(ptr)
    {
        if (GetPtr() == 0) std::cerr << "UNRESOLVED SIG: " << GetSig() << std::endl;
    }

    Function(const char* ptr) noexcept : Signature(ptr)
    {
        if (GetPtr() == 0) std::cerr << "UNRESOLVED SIG: " << GetSig() << std::endl;
    }

    Function(Signature sig) noexcept : Signature(sig)
    {
        if (GetPtr() == 0) std::cerr << "UNRESOLVED SIG: " << GetSig() << std::endl;
    }

    Ret Execute(Args... args)
    {
        using FunctionFn = Ret(__fastcall*)(Args...);
        FunctionFn OnFunction = reinterpret_cast<FunctionFn>(GetPtr());
        return OnFunction(std::forward<Args>(args)...);
    }

    Ret operator()(Args... args)
    {
        return Execute(args...);
    }
};