/*----------------------------------------------------------------------------*/
/*                                                                            */
/*    Copyright (c) Aaron Wilk 2025, All rights reserved.                     */
/*                                                                            */
/*    Module:     Hook.h			                                          */
/*    Author:     Aaron Wilk                                                  */
/*    Created:    25 June 2025                                                */
/*                                                                            */
/*    Revisions:  V0.5                                                        */
/*                                                                            */
/*----------------------------------------------------------------------------*/

#pragma once
#include "MinHook/MinHook.h"
#include "Signature.hpp"
#include <cassert>
#include <utility>

/// Forward declaration allowing a pseudo-signature to use as template arguments.
template <typename>
class Hook;

/// Class representing a Hook. Smart abstraction layer over MinHook.
/// @tparam Ret Return type of the function to Hook.
/// @tparam Args Arguments of the function to Hook.
template <typename Ret, typename... Args>
class Hook<Ret(Args...)> final : public Function<Ret(Args...)>
{
public:

	using TFunction = Ret(*)(Args...);

	/// Creates a Hook object using the specified BR-SDK compatible signature.
	/// @param signature BR-SDK compatible signature. See UsingBRSDK.MD Signature Formatting
	/// @param hookFunc Valid trampoline function pointer matching the signature specified in the Hook objects template signature.
	Hook(const char* signature, TFunction hookFunc) noexcept;

	/// Creates a Hook object using the specified BR-SDK compatible signature.
	/// @param signature BR-SDK Signature. See UsingBRSDK.MD Signature Formatting
	/// @param hookFunc Valid trampoline function pointer matching the signature specified in the Hook objects template signature.
	Hook(const Signature& signature, TFunction hookFunc) noexcept;

	/// Creates a Hook object using the specified address and trampoline function. Does not register the Hook with MinHook. Use Create() to register with MinHook.
	/// @param address Address of the function to hook.
	/// @param hookFunc Valid trampoline function pointer matching the signature specified in the Hook objects template signature.
	Hook(unsigned long long address, TFunction hookFunc) noexcept;

	/// Creates a Hook object using the specified function pointer and trampoline function. Does not register the Hook with MinHook. For VTable entries use reinterpret_cast<Ret(__fastcall*)(Args...)>(vtable[index]).
	/// @param pointer Function pointer of the specified signature in the Hook objects template signature.
	/// @param hookFunc Valid trampoline function pointer matching the signature specified in the Hook objects template signature.
	Hook(Ret(__fastcall* pointer)(Args...), TFunction hookFunc) noexcept;

	/// Explicit deconstructor for Hook. Disables then removes the Hook with MinHook. It is suggested to call Disable() or HOOK_DISABLE() instead of calling this if using the HOOK() macro to define the Hook.
	virtual ~Hook();

	Hook(const Hook&)						= delete;
	Hook& operator=(const Hook&)			= delete;
	Hook(Hook&&) noexcept					= delete;
	Hook& operator=(Hook&& other) noexcept  = delete;

private:
	std::atomic_bool Enabled;
	std::atomic_bool Initialized;

	/// Initializes the Hook with MinHook and Searches patterns if necessary.
	/// @return Whether initialization completed successfully. Called in Create().
	bool Init();

public:
	TFunction OriginalFunction;

protected:
	TFunction HookedFunction;

public:

	/// Registers the Hook with MinHook but does not enable it
	void Create();

	/// Enables the Hook.
	void Enable();

	/// Disables the Hook.
	void Disable();

	/// Destroys the Hook.
	void Destroy();

	/// Calls the original function of the Hook, bypassing the trampoline
	/// @param args Args of the function as defined in the Hook objects template signature
	/// @return The return type defined in the Hook objects template signature
	Ret CallOriginalFunction(Args... args);

	Ret CallOriginal(Args... args)
	{
		return CallOriginalFunction(args...);
	}

	// Calls the hooked function variant. Should also result in the () operator calling the Call() on Hook
	Ret Call(Args... args) override;

	/// Whether the Hook has been registered with MinHook
	/// @return Initialization state of the Hook in respect to its MinHook registration.
	bool IsInitialized();

	/// Whether the Hook has been Created.
	/// @return Initialization state of the Hook in respect to its MinHook registration.
	bool IsCreated() { return IsInitialized(); }

	/// Whether the Hook is enabled
	/// @return Enabled state of the Hook.
	bool IsEnabled();

	void Toggle(bool toggle);
};

template <typename Ret, typename ... Args>
Hook<Ret(Args...)>::Hook(const char* signature, TFunction hookFunc) noexcept : Function<Ret(Args...)>(signature)
{
	Enabled = false;
	Initialized = false;
	OriginalFunction = nullptr;
	HookedFunction = hookFunc;
}

template<typename Ret, typename ...Args>
Hook<Ret(Args...)>::Hook(const Signature& signature, TFunction hookFunc) noexcept : Function<Ret(Args...)>(signature)
{
	Enabled = false;
	Initialized = false;
	OriginalFunction = nullptr;
	HookedFunction = hookFunc;
}

template<typename Ret, typename ...Args>
Hook<Ret(Args...)>::Hook(unsigned long long addr, TFunction hookFunc) noexcept : Function<Ret(Args...)>(addr)
{
	Enabled = false;
	Initialized = false;
	OriginalFunction = nullptr;
	HookedFunction = hookFunc;
}

template<typename Ret, typename ...Args>
Hook<Ret(Args...)>::Hook(Ret(__fastcall* pointer)(Args...), TFunction hookFunc) noexcept : Function<Ret(Args...)>(reinterpret_cast<unsigned long long>(pointer))
{
	Enabled = false;
	Initialized = false;
	OriginalFunction = nullptr;
	HookedFunction = hookFunc;
}

template<typename Ret, typename ...Args>
Hook<Ret(Args...)>::~Hook()
{
	Disable();
	Destroy();
	OriginalFunction = nullptr;
}

template<typename Ret, typename ...Args>
bool Hook<Ret(Args...)>::Init() {
	if (Initialized) return true;
	if (this->GetPtr() == 0) return false;
	MH_STATUS ret = MH_CreateHook((LPVOID)this->GetPtr(), HookedFunction, (void**)&OriginalFunction);
	Initialized = ret == MH_OK;
	return ret == MH_OK;
}

template<typename Ret, typename ...Args>
void Hook<Ret(Args...)>::Create()
{
	if (Initialized) return;
	assert(Init());
}

template<typename Ret, typename ...Args>
void Hook<Ret(Args...)>::Enable()
{
	if (!Initialized) Create();
	if (!Initialized || Enabled) return;
	MH_QueueEnableHook((LPVOID)this->GetPtr());
	MH_ApplyQueued();
	Enabled = true;
}

template<typename Ret, typename ...Args>
void Hook<Ret(Args...)>::Disable()
{
	if (!Initialized || !Enabled) return;
	MH_QueueDisableHook((LPVOID)this->GetPtr());
	MH_ApplyQueued();
	Enabled = false;
}

template<typename Ret, typename ...Args>
void Hook<Ret(Args...)>::Destroy()
{
	if (!Initialized) return;
	if (!Enabled) Disable();
	MH_RemoveHook((LPVOID)this->GetPtr());
	Initialized = false;
	Enabled = false;
}

template<typename Ret, typename ...Args>
Ret Hook<Ret(Args...)>::CallOriginalFunction(Args ...args)
{
	assert(OriginalFunction != nullptr);
	return OriginalFunction(std::forward<Args>(args)...);
}

template<typename Ret, typename ...Args>
Ret Hook<Ret(Args...)>::Call(Args ...args)
{
	assert(HookedFunction != nullptr);
	return HookedFunction(std::forward<Args>(args)...);
}

template<typename Ret, typename ...Args>
bool Hook<Ret(Args...)>::IsInitialized()
{
	return Initialized;
}

template<typename Ret, typename ...Args>
bool Hook<Ret(Args...)>::IsEnabled()
{
	return Enabled;
}

template<typename Ret, typename ...Args>
void Hook<Ret(Args...)>::Toggle(bool toggle)
{
	if (toggle) Enable();
	else Disable();
}

