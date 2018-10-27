#pragma once

#include <Windows.h>


typedef struct _HookData
{
	WCHAR szHookedModuleName[MAX_PATH];
	WCHAR szHookedClass[MAX_PATH];
	WCHAR szHookedFunction[MAX_PATH];
	ULONG nHookedFunctionArgumentsCount;
	UINT_PTR uiHookedFunctionId;

	WCHAR szHookModuleName[MAX_PATH];
	WCHAR szHookClass[MAX_PATH];
	WCHAR szHookFunction[MAX_PATH];
	WCHAR szHookDummyFunction[MAX_PATH];
	ULONG nHookFunctionArgumentsCount;
	UINT_PTR miHookedModuleId;
	UINT_PTR miHookModuleId;
	UINT_PTR uiHookFunctionId;
	UINT_PTR uiDummyFunctionId;
	DWORD bHookedJITed;
	DWORD bHookJITed;
	DWORD bDummyJITed;
	BOOL bHooked;
} HookData, *PHookData;
