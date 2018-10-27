// Copyright (c) Javelin-Networks
// Modified version of .Net ClrProfiler code for Babel-Shellfish


// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "BabelShellfishProfiler.h"
#include "MinHook_src\include\MinHook.h"
#include "HookData.h"

#include <corhlpr.h>
#include <string>
#include <Windows.h>

#include <cor.h>
#include <corprof.h>
#include <corpub.h>
#include <MetaHost.h>


HookData g_HookData[] =
{
	///////////////////////////////////////////////////////////
	/// BabelShellfish hooks
	{ L"System.Management.Automation", L"System.Management.Automation.AmsiUtils", L"ScanContent", 2, 0,
	L"Babel-Shellfish", L"BabelShellfish.BabelShellfish", L"ScanContent", L"ScanContentDummy", 2, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ L"System.Management.Automation", L"System.Management.Automation.NativeCommandProcessor", L"GetProcessStartInfo", 4, 0,
	L"Babel-Shellfish", L"BabelShellfish.BabelShellfish", L"GetProcessStartInfo", L"GetProcessStartInfoDummy", 5, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ L"System.Management.Automation", L"System.Management.Automation.DlrScriptCommandProcessor", L"RunClause", 3, 0,
	L"Babel-Shellfish", L"BabelShellfish.BabelShellfish", L"RunClause", L"RunClauseDummy", 4, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ L"System.Management.Automation", L"System.Management.Automation.Cmdlet", L"DoProcessRecord", 0, 0,
	L"Babel-Shellfish", L"BabelShellfish.BabelShellfish", L"DoProcessRecord", L"DoProcessRecordDummy", 1, 0, 0, 0, 0, 0, 0, 0, 0 },

	{ L"System.Management.Automation", L"System.Management.Automation.Language.PSInvokeMemberBinder", L"InvokeMethod", 5, 0,
	L"Babel-Shellfish", L"BabelShellfish.BabelShellfish", L"InvokeMethod", L"InvokeMethodDummy", 5, 0, 0, 0, 0, 0, 0, 0, 0 },
	// TODO: Add more hooks on the compiler
	//{ L"System.Management.Automation.dll", L"System.Management.Automation.Internal.ClassOps", L"CallMethodNonVirtuallyImpl", 3,
	//L"TestProject.dll", L"TestProject.TestProject", L"CallMethodNonVirtuallyImpl", L"CallMethodNonVirtuallyImplDummy", 3 },

	/// BabelShellfish hook
	///////////////////////////////////////////////////////////
};

ULONG g_nHookData = sizeof(g_HookData) / sizeof(HookData);

BabelShellfishProfiler::BabelShellfishProfiler() : refCount(0), corProfilerInfo(nullptr)
{
	this->bDetachInitiazted = false;

	// Initialize MinHook engine
	MH_STATUS status;
	status = MH_Initialize();
	if (MH_OK != status)
	{
		if (MH_ERROR_ALREADY_INITIALIZED == status)
		{
			MyDebugPrintA("MH_Initialize status: MH_ERROR_ALREADY_INITIALIZED\n");
		}
		else
		{
			MyDebugPrintA("MH_Initialize Failed status: %d\n");
		}
	}

	MyDebugPrintA("MH_Initialize status ok\n");

}

BabelShellfishProfiler::~BabelShellfishProfiler()
{
	if (this->corProfilerInfo != nullptr)
    {
        this->corProfilerInfo->Release();
        this->corProfilerInfo = nullptr;
    }

}



DWORD BabelShellfishProfiler::DetachProfilerThread(ICorProfilerInfo3* corProfilerInfo)
{
	DWORD hr = E_FAIL;
	MyDebugPrintA("Detach thread\n");

	HMODULE hMod = LoadLibraryA("BabelShellfishProfiler.dll");

	if (NULL == hMod)
	{
		MyDebugPrintA("Failed getting handle to ClrProfiler!\n");
		goto Leave;
	}

	if (NULL != corProfilerInfo)
	{
		HRESULT hr = corProfilerInfo->RequestProfilerDetach(1000);
		if (S_OK != hr)
		{
			MyDebugPrintA("Cannot detach profiler :( 0x%X\n", hr);
		}
	}

	FreeLibraryAndExitThread(hMod, 0);

	hr = S_OK;

Leave:
	return hr;
}

HRESULT BabelShellfishProfiler::DetachProfiler()
{
	if (!bDetachInitiazted)
	{
		bDetachInitiazted = true;

		DWORD threadId = 0;
		if (FALSE == CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)DetachProfilerThread, (LPVOID)corProfilerInfo, 0, &threadId))
		{
			MyDebugPrintA("Failed running detach thread!\n");
			return E_FAIL;
		}
		MyDebugPrintA("Success running detach thread!\n");

	}

	return S_OK;
}
UINT_PTR BabelShellfishProfiler::GetJittedFunctionAddress(FunctionID functionId)
{
	UINT_PTR retVal = 0;

	COR_PRF_CODE_INFO codeInfos[20] = { 0 };
	ULONG32 cCodeInfo = 20;
	if (S_OK == this->corProfilerInfo->GetCodeInfo2(functionId, cCodeInfo, &cCodeInfo, codeInfos))
	{
		// GetCodeInfo2 returns an array of address sorted by the IL code so the first member is the beginning of the code
		retVal = codeInfos[0].startAddress;
	}
	else
	{
		MyDebugPrintW(L"GetCodeInfo2 failed\n");
	}

	return retVal;

}
// Finds the class in requested module and enumerates all methods until it finds the function
// Enumeration is a must for pre-JIT-ed methods since they won't show up on JIT events
UINT_PTR BabelShellfishProfiler::FindJittedFunctionAddress(ModuleID moduleId, LPCWSTR szClassName, LPCWSTR szFuncName, ULONG nParamsCount)
{
	UINT_PTR retVal = 0;
	IUnknown *iUnknown = NULL;
	if (S_OK != this->corProfilerInfo->GetModuleMetaData(moduleId, ofRead, IID_IMetaDataImport, &iUnknown))
	{
		MyDebugPrintW(L"GetModuleMetaData error\n");
		goto Leave;
	}

	IMetaDataImport *metaData = (IMetaDataImport*)iUnknown;

	// Fetch the class TypeDef
	mdTypeDef mdScriptBlock = { 0 };
	ULONG nDefs = 0;
	if (S_OK == metaData->FindTypeDefByName(szClassName, NULL, &mdScriptBlock))
	{
		// Fetch the ClassID
		ClassID classID;
		if (S_OK != this->corProfilerInfo->GetClassFromTokenAndTypeArgs(moduleId, mdScriptBlock, 0, NULL, &classID))
		{
			MyDebugPrintW(L"GetClassFromTokenAndTypeArgs failed\n");
			goto Leave;
		}

		// Enumerate all methods of class
		HCORENUM pMethodDefsEnum = NULL;
		mdMethodDef methodsDef;
		ULONG nMethodDefs = 1;
		HRESULT res = S_OK;
		do
		{
			res = metaData->EnumMethods(&pMethodDefsEnum, mdScriptBlock, &methodsDef, nMethodDefs, &nMethodDefs);
			if (nMethodDefs > 0)
			{
				// Fetch method's properties
				mdTypeDef           mdClass;
				WCHAR              szMethod[500];
				ULONG               cchMethod = 500;
				DWORD               dwAttr;
				PCCOR_SIGNATURE     pvSigBlob;
				ULONG               cbSigBlob = 0;
				ULONG               ulCodeRVA;
				DWORD               dwImplFlags;
				if (S_OK == metaData->GetMethodProps(methodsDef, &mdClass, szMethod, cchMethod, &cchMethod, &dwAttr, &pvSigBlob, &cbSigBlob, &ulCodeRVA, &dwImplFlags))
				{

					MyDebugPrintW(szMethod);

					// Check if requested function was found
					if (0 == wcscmp(szMethod, szFuncName))
					{
						MyDebugPrintW(szMethod);
						HCORENUM paramsEnum = NULL;
						mdParamDef mdParams[20] = { 0 };
						ULONG nParams = 20;
						// Fetch the params count to make sure we got the correct function
						// TODO: Add support for parameters type instead of just count
						HRESULT paramsRes = metaData->EnumParams(&paramsEnum, methodsDef, mdParams, nParams, &nParams);
						if (nParamsCount == nParams)
						{
							// Function Found! Get JITed code address
							FunctionID funcID;
							if (S_OK == this->corProfilerInfo->GetFunctionFromTokenAndTypeArgs(moduleId, methodsDef, classID, 0, NULL, &funcID))
							{
								retVal = this->GetJittedFunctionAddress(funcID);
							}
						}

						metaData->CloseEnum(paramsEnum);
					}

				}
			}
		} while ((S_OK == res) && (0 == retVal));
		metaData->CloseEnum(pMethodDefsEnum);
	}
	else
	{
		MyDebugPrintW(L"FindTypeDefByName error\n");
	}

Leave:
	return retVal;
}



bool BabelShellfishProfiler::PlaceNetHook(UINT_PTR hooked, UINT_PTR hook, UINT_PTR dummy)
{
	bool retVal = false;

	LPVOID lpOriginalHook = NULL;
	LPVOID lpOriginalDummy = NULL;
	MyDebugPrintA("Placing hook: original: %p, hook: %p, dummy: %p\n", hooked, hook, dummy);

	// Redirect all calls to original (hooked) functions to hook function
	MH_STATUS status = MH_CreateHook(
		(LPVOID)hooked,
		(LPVOID)hook,
		&lpOriginalHook
	);
	if (MH_OK != status)
	{
		MyDebugPrintA("MH_CreateHook Failed (hook) %d: original: %p, hook: %p, trampoline: %p\n", status, hooked, hook, lpOriginalHook);
		goto Leave;
	}

	// Replace a dummy function in hook module to allow the hook function call back to original function
	status = MH_CreateHook(
		(LPVOID)dummy,
		(LPVOID)lpOriginalHook,
		&lpOriginalDummy
	);
	if (MH_OK != status)
	{
		MyDebugPrintA("MH_CreateHook Failed (dummy) %d: original: %p, hook: %p, trampoline: %p\n", status, dummy, lpOriginalHook, lpOriginalDummy);
		goto Leave;
	}
	MyDebugPrintA("Success placing hook %d: original: %p, hook: %p, dummy: %p, trampoline: %p\n", status, hooked, hook, dummy, lpOriginalHook);

	if (MH_EnableHook((LPVOID)hooked) != MH_OK)
	{
		MyDebugPrintW(L"MH_EnableHook (hook) Failed\n");
		goto Leave;
	}
	if (MH_EnableHook((LPVOID)dummy) != MH_OK)
	{
		MyDebugPrintW(L"MH_EnableHook (dummy) Failed\n");
		goto Leave;
	}

	retVal = true;

Leave:
	return retVal;

}


HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::Initialize(IUnknown *pICorProfilerInfoUnk)
{
	MyDebugPrintA("Babel-Shellfish In BabelShellfishProfiler::Initialize\n");

	HKEY hkBabelShellfishPath;
	DWORD cbDataSize = MAX_PATH * sizeof(WCHAR);
	LSTATUS res = ERROR_SUCCESS;

	if (ERROR_SUCCESS != RegOpenKeyExW(
		HKEY_CLASSES_ROOT,
		L"CLSID\\{cf0d821e-299b-5307-a3d8-b283c03916db}",
		0,
		KEY_QUERY_VALUE,
		&hkBabelShellfishPath))
	{
		MyDebugPrintA("Failed to find Babel-Shellfish configuration hive (%x)\n", GetLastError());
		return E_FAIL;
	}
	res = RegGetValueW(
		hkBabelShellfishPath,
		L"Config",
		L"BabelShellfishPath",
		RRF_RT_REG_EXPAND_SZ | RRF_RT_REG_SZ,
		NULL,
		this->szBabelShellfishPath,
		&cbDataSize);

	RegCloseKey(hkBabelShellfishPath);

	if (ERROR_SUCCESS != res)
	{
		MyDebugPrintA("Failed to find Babel-Shellfish path (%x)\n", GetLastError());
		return E_FAIL;

	}


	HRESULT queryInterfaceResult = pICorProfilerInfoUnk->QueryInterface(__uuidof(ICorProfilerInfo3), reinterpret_cast<void **>(&this->corProfilerInfo));

	if (FAILED(queryInterfaceResult))
	{
		MyDebugPrintA("Failed to Initialize ICorProfilerInfo3 %x\n", queryInterfaceResult);
		return E_FAIL;
	}

	DWORD eventMask =  COR_PRF_MONITOR_MODULE_LOADS;

    HRESULT hr = this->corProfilerInfo->SetEventMask(eventMask);

	if (FAILED(hr))
	{
		MyDebugPrintA("Failed to Initialize %x\n", hr);
	}
	
	return hr;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::Shutdown()
{
    if (this->corProfilerInfo != nullptr)
    {
        this->corProfilerInfo->Release();
        this->corProfilerInfo = nullptr;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::AppDomainCreationStarted(AppDomainID appDomainId)
{
    return S_OK;
}

void LoadBabelShellfish(WCHAR *szBabelShellfishPath)
{
	MyDebugPrintA("In LoadBabelShellfish\n");


	ICLRMetaHost * pMetaHost = NULL;
	IEnumUnknown * pEnum = NULL;
	IUnknown * pUnk = NULL;
	ICLRRuntimeInfo * pRuntime = NULL;
	ICLRRuntimeHost * pHost = NULL;
	HRESULT hr = E_FAIL;
	BOOLEAN nRunCount = 2;

	// In case thread exits after the profiler finished loading
	HMODULE hBabelShellfishMod = LoadLibraryA("BabelShellfishProfiler.dll");

	if (NULL == hBabelShellfishMod)
	{
		MyDebugPrintA("Failed getting handle to BabelShellfishProfiler.dll when loading BabelShellfish!\n");
		goto Cleanup;
	}


	MyDebugPrintA("\t[+] Loading mscoree.dll\n");
	HMODULE hModule = GetModuleHandleW(L"mscoree.dll");
	if (hModule == NULL)
		goto Cleanup;

	CLRCreateInstanceFnPtr pfnCreateInterface =
		(CLRCreateInstanceFnPtr)GetProcAddress(hModule, "CLRCreateInstance");
	if (pfnCreateInterface == NULL)
		goto Cleanup;

	MyDebugPrintA("\t[+] Fetching CLR interface\n");
	hr = (*pfnCreateInterface)(CLSID_CLRMetaHost, IID_ICLRMetaHost, (LPVOID *)&pMetaHost);
	if (FAILED(hr))
	{
		MyDebugPrintA("\t[+]\tFailed (%X)\n", hr);
		goto Cleanup;
	}

	MyDebugPrintA("\t[+] Enumerating loaded runtimes\n");
	hr = pMetaHost->EnumerateLoadedRuntimes(GetCurrentProcess(), &pEnum);
	if (FAILED(hr))
	{
		MyDebugPrintA("\t[+]\tFailed (%X)\n", hr);
		goto Cleanup;
	}


	while (pEnum->Next(1, &pUnk, NULL) == S_OK)
	{
		hr = pUnk->QueryInterface(IID_ICLRRuntimeInfo, (LPVOID *)&pRuntime);
		if (FAILED(hr))
		{
			MyDebugPrintA("\t[+] Failed (%X)\n", hr);
			pUnk->Release();
			pUnk = NULL;
			continue;
		}
		WCHAR wszVersion[30];
		DWORD cchVersion = sizeof(wszVersion) / sizeof(wszVersion[0]);
		hr = pRuntime->GetVersionString(wszVersion, &cchVersion);
		if (SUCCEEDED(hr))
		{
			MyDebugPrintA("\t[+] Found CLR %S\n", wszVersion);

			if ((cchVersion >= 3) &&
				((wszVersion[0] == L'v') || (wszVersion[0] == L'V')) &&
				((wszVersion[1] >= L'4') || (wszVersion[2] != L'.')))
			{
				MyDebugPrintA("\t[+] Injecting Babel-Shellfish.dll...\n");

				if (S_OK == pRuntime->GetInterface(CLSID_CLRRuntimeHost, IID_ICLRRuntimeHost, (LPVOID *)&pHost))
				{
					DWORD dwRet;
					HRESULT hrExec = pHost->ExecuteInDefaultAppDomain(
						szBabelShellfishPath,
						L"BabelShellfish.BabelShellfish",
						L"Run",
						L"",
						&dwRet);
					MyDebugPrintA("\t[+] ExecuteInDefaultAppDomain result: %x", hrExec);

					pHost->Release();
					pHost = NULL;
				}

				pRuntime->Release();
				pRuntime = NULL;
				pUnk->Release();
				pUnk = NULL;

				if (SUCCEEDED(hr))
				{
					break;
				}
			}
		}
	}
	if (pUnk != NULL)
	{
		pUnk->Release();
		pUnk = NULL;
	}

Cleanup:
	if (NULL != pHost)
	{
		pHost->Release();
		pHost = NULL;
	}
	if (pRuntime != NULL)
	{
		pRuntime->Release();
		pRuntime = NULL;
	}

	if (pUnk != NULL)
	{
		pUnk->Release();
		pUnk = NULL;
	}

	if (pEnum != NULL)
	{
		pEnum->Release();
		pEnum = NULL;
	}

	if (pMetaHost != NULL)
	{
		pMetaHost->Release();
		pMetaHost = NULL;
	}

	if (hModule != NULL)
	{
		// No need to call FreeLibrary since it was opened with GetModuleHandle()
		hModule = NULL;
	}
	FreeLibraryAndExitThread(hBabelShellfishMod, 0);

}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::AppDomainCreationFinished(AppDomainID appDomainId, HRESULT hrStatus)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::AppDomainShutdownStarted(AppDomainID appDomainId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::AppDomainShutdownFinished(AppDomainID appDomainId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::AssemblyLoadStarted(AssemblyID assemblyId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::AssemblyLoadFinished(AssemblyID assemblyId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::AssemblyUnloadStarted(AssemblyID assemblyId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::AssemblyUnloadFinished(AssemblyID assemblyId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ModuleLoadStarted(ModuleID moduleId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ModuleLoadFinished(ModuleID moduleId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ModuleUnloadStarted(ModuleID moduleId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ModuleUnloadFinished(ModuleID moduleId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ModuleAttachedToAssembly(ModuleID moduleId, AssemblyID assemblyId)
{
	MyDebugPrintA("ModuleAttachedToAssembly, AssemblyID: %x", assemblyId);
	WCHAR szName[MAX_PATH];
	ULONG cchName = MAX_PATH;
	AppDomainID adAppDomainId;
	ModuleID miModuleId;


	HRESULT hr = this->corProfilerInfo->GetAssemblyInfo(
		assemblyId,
		cchName,
		&cchName,
		szName,
		&adAppDomainId,
		&miModuleId);
	if (S_OK != hr || cchName > MAX_PATH)
	{
		MyDebugPrintA("Failed getting assembly name, AssemblyID: %x, HRESULT: %X, Length: %x", assemblyId, hr, cchName);
		goto Leave;
	}
	MyDebugPrintA("Found assembly %S", szName);

	for (DWORD i = 0; i < g_nHookData; ++i)
	{
		if (0 == g_HookData[i].miHookedModuleId
			&& 0 == _wcsicmp(g_HookData[i].szHookedModuleName, szName))
		{
			MyDebugPrintA("Found hook assembly %S for %S::%S", szName, g_HookData[i].szHookedClass, g_HookData[i].szHookedFunction);

			g_HookData[i].miHookedModuleId = miModuleId;
		}
		if (0 == g_HookData[i].miHookModuleId
			&& 0 == _wcsicmp(g_HookData[i].szHookModuleName, szName))
		{
			MyDebugPrintA("Found hook assembly %S for %S::%S", szName, g_HookData[i].szHookClass, g_HookData[i].szHookFunction);
			g_HookData[i].miHookModuleId = miModuleId;
		}
	}

	static DWORD threadId = 0;
	if ((0 != g_HookData[0].miHookedModuleId)
		&& (0 == threadId))
	{
		// Start monitoring JIT events to start hooking
		DWORD eventMask = 0
			| COR_PRF_MONITOR_MODULE_LOADS
			| COR_PRF_MONITOR_JIT_COMPILATION;

		HRESULT hr = this->corProfilerInfo->SetEventMask(eventMask);

		if (FAILED(hr))
		{
			MyDebugPrintA("Failed to monitor JIT compilation (error: %x)\n", hr);
			goto Leave;
		}

		if (FALSE == CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LoadBabelShellfish, this->szBabelShellfishPath, 0, &threadId))
		{
			MyDebugPrintA("Failed running LoadBabelShellfish thread!\n");
		}
		else
		{
			MyDebugPrintA("Success running LoadBabelShellfish thread!\n");
		}
	}

Leave:

	return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ClassLoadStarted(ClassID classId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ClassLoadFinished(ClassID classId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ClassUnloadStarted(ClassID classId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ClassUnloadFinished(ClassID classId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::FunctionUnloadStarted(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::JITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::JITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock)
{
	MyDebugPrintA("JITCompilationFinished, functionId: %x\n", functionId);
	ClassID ciClass = 0;
	ModuleID miModule = 0;
	mdToken mdToken = 0;
	mdTypeDef mdClass = 0;
	IMetaDataImport* metaDataImport = NULL;
	HRESULT hr = S_OK;
	WCHAR szFunctionName[MAX_PATH] = { 0 };
	ULONG cchFunctionName = MAX_PATH;
	WCHAR szClassName[MAX_PATH] = { 0 };
	ULONG cchClassName = MAX_PATH;
	UINT_PTR lpHookFunction = 0;

	// Find class and method's name, then fetch the JIT-ed address
	hr = this->corProfilerInfo->GetFunctionInfo(
		functionId,
		&ciClass,
		&miModule,
		&mdToken);
	if (SUCCEEDED(hr))
	{
		hr = this->corProfilerInfo->GetModuleMetaData(miModule, ofRead, IID_IMetaDataImport, (IUnknown**)&metaDataImport);
		if (SUCCEEDED(hr))
		{
			hr = metaDataImport->GetMethodProps(mdToken, &mdClass, szFunctionName, cchFunctionName, &cchFunctionName, NULL, NULL, NULL, NULL, NULL);
			if (SUCCEEDED(hr))
			{
				hr = metaDataImport->GetTypeDefProps(mdClass, szClassName, cchClassName, &cchClassName, NULL, NULL);
				if (SUCCEEDED(hr))
				{
					MyDebugPrintA("JITCompilationFinished, name: %S.%S\n", szClassName, szFunctionName);
					lpHookFunction = this->GetJittedFunctionAddress(functionId);
				}
			}
			metaDataImport->Release();
		}
	}

	DWORD nHookedFunctions = 0;

	if (0 != lpHookFunction)
	{
		// Look if JIT-ed method matches any of our hook functions
		for (DWORD i = 0; i < g_nHookData; ++i)
		{
			// Skip if already hooked
			if (g_HookData[i].bHooked)
			{
				++nHookedFunctions;
				continue;
			}
			// If hook dll was not loaded yet
			if (0 == g_HookData[i].miHookedModuleId
				&& 0 == g_HookData[i].miHookModuleId)
			{
				continue;
			}
			// Compare JIT-ed method to the hook function
			if ((0 == g_HookData[i].uiHookedFunctionId)
				&& (0 == _wcsicmp(g_HookData[i].szHookClass, szClassName))
				&& (0 == _wcsicmp(g_HookData[i].szHookedFunction, szFunctionName)))
			{
				MyDebugPrintA("Found hook function\n");
				g_HookData[i].uiHookFunctionId = lpHookFunction;
			}
			// Compare JIT-ed method to the dummy function
			else if ((0 == g_HookData[i].uiDummyFunctionId)
				&& (0 == _wcsicmp(g_HookData[i].szHookClass, szClassName))
				&& (0 == _wcsicmp(g_HookData[i].szHookDummyFunction, szFunctionName)))
			{
				MyDebugPrintA("Found dummy function\n");
				g_HookData[i].uiDummyFunctionId = lpHookFunction;
			}

			// If both hook and dummy methods were found, look for the original method
			if ((0 != g_HookData[i].uiHookFunctionId)
				&& (0 != g_HookData[i].uiDummyFunctionId))
			{
				g_HookData[i].uiHookedFunctionId = FindJittedFunctionAddress(
					g_HookData[i].miHookedModuleId,
					g_HookData[i].szHookedClass,
					g_HookData[i].szHookedFunction,
					g_HookData[i].nHookedFunctionArgumentsCount);
			}

			// if all methods were found, place the hook
			if (0 == g_HookData[i].uiHookFunctionId
				|| 0 == g_HookData[i].uiDummyFunctionId
				|| 0 == g_HookData[i].uiHookedFunctionId)
			{
				continue;
			}
			// All JIT addresses were found, hook the functions
			if (PlaceNetHook(g_HookData[i].uiHookedFunctionId, g_HookData[i].uiHookFunctionId, g_HookData[i].uiDummyFunctionId))
			{
				MyDebugPrintA("Hook success!");
				g_HookData[i].bHooked = TRUE;
				++nHookedFunctions;
			}

		}
	}

	if (nHookedFunctions == g_nHookData)
	{
		MyDebugPrintA("All hooks in place, calling DetachProfiler");
		this->DetachProfiler();
	}

    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::JITCachedFunctionSearchStarted(FunctionID functionId, BOOL *pbUseCachedFunction)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::JITCachedFunctionSearchFinished(FunctionID functionId, COR_PRF_JIT_CACHE result)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::JITFunctionPitched(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::JITInlining(FunctionID callerId, FunctionID calleeId, BOOL *pfShouldInline)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ThreadCreated(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ThreadDestroyed(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RemotingClientInvocationStarted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RemotingClientSendingMessage(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RemotingClientReceivingReply(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RemotingClientInvocationFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RemotingServerReceivingMessage(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RemotingServerInvocationStarted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RemotingServerInvocationReturned()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RemotingServerSendingReply(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::UnmanagedToManagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ManagedToUnmanagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RuntimeSuspendStarted(COR_PRF_SUSPEND_REASON suspendReason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RuntimeSuspendFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RuntimeSuspendAborted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RuntimeResumeStarted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RuntimeResumeFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RuntimeThreadSuspended(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RuntimeThreadResumed(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::MovedReferences(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], ULONG cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ObjectAllocated(ObjectID objectId, ClassID classId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ObjectsAllocatedByClass(ULONG cClassCount, ClassID classIds[], ULONG cObjects[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ObjectReferences(ObjectID objectId, ClassID classId, ULONG cObjectRefs, ObjectID objectRefIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RootReferences(ULONG cRootRefs, ObjectID rootRefIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionThrown(ObjectID thrownObjectId)
{

    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionSearchFunctionEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionSearchFunctionLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionSearchFilterEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionSearchFilterLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionSearchCatcherFound(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionOSHandlerEnter(UINT_PTR __unused)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionOSHandlerLeave(UINT_PTR __unused)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionUnwindFunctionEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionUnwindFunctionLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionUnwindFinallyEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionUnwindFinallyLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionCatcherEnter(FunctionID functionId, ObjectID objectId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionCatcherLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::COMClassicVTableCreated(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable, ULONG cSlots)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::COMClassicVTableDestroyed(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionCLRCatcherFound()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ExceptionCLRCatcherExecute()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ThreadNameChanged(ThreadID threadId, ULONG cchName, WCHAR name[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::GarbageCollectionStarted(int cGenerations, BOOL generationCollected[], COR_PRF_GC_REASON reason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::SurvivingReferences(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], ULONG cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::GarbageCollectionFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::FinalizeableObjectQueued(DWORD finalizerFlags, ObjectID objectID)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::RootReferences2(ULONG cRootRefs, ObjectID rootRefIds[], COR_PRF_GC_ROOT_KIND rootKinds[], COR_PRF_GC_ROOT_FLAGS rootFlags[], UINT_PTR rootIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::HandleCreated(GCHandleID handleId, ObjectID initialObjectId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::HandleDestroyed(GCHandleID handleId)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::InitializeForAttach(IUnknown *pCorProfilerInfoUnk, void *pvClientData, UINT cbClientData)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ProfilerAttachComplete()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ProfilerDetachSucceeded()
{
    return S_OK;
}


HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ReJITCompilationStarted(FunctionID functionId, ReJITID rejitId, BOOL fIsSafeToBlock)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::GetReJITParameters(ModuleID moduleId, mdMethodDef methodId, ICorProfilerFunctionControl *pFunctionControl)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ReJITCompilationFinished(FunctionID functionId, ReJITID rejitId, HRESULT hrStatus, BOOL fIsSafeToBlock)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ReJITError(ModuleID moduleId, mdMethodDef methodId, FunctionID functionId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::MovedReferences2(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], SIZE_T cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::SurvivingReferences2(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], SIZE_T cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE BabelShellfishProfiler::ConditionalWeakTableElementReferences(ULONG cRootRefs, ObjectID keyRefIds[], ObjectID valueRefIds[], GCHandleID rootIds[])
{
    return S_OK;
}

