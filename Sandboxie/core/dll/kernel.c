/*
 * Copyright 2021-2024 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
// Kernel
//---------------------------------------------------------------------------

//#define NOGDI
//#include <windows.h>
//#include "common/win32_ntddk.h"
#include "dll.h"

#define CONF_LINE_LEN               2000    // keep in sync with drv/conf.c

//---------------------------------------------------------------------------
// Functions Prototypes
//---------------------------------------------------------------------------

typedef LPWSTR (*P_GetCommandLineW)(VOID);

typedef LPSTR (*P_GetCommandLineA)(VOID);

typedef EXECUTION_STATE (*P_SetThreadExecutionState)(EXECUTION_STATE esFlags);

typedef DWORD(*P_GetTickCount)();

typedef ULONGLONG (*P_GetTickCount64)();

typedef BOOL(*P_QueryUnbiasedInterruptTime)(PULONGLONG UnbiasedTime);

//typedef void(*P_Sleep)(DWORD dwMiSecond);

typedef DWORD(*P_SleepEx)(DWORD dwMiSecond, BOOL bAlert);

typedef BOOL (*P_QueryPerformanceCounter)(LARGE_INTEGER* lpPerformanceCount);

typedef LANGID (*P_GetUserDefaultUILanguage)();

typedef int (*P_GetUserDefaultLocaleName)(
	 LPWSTR lpLocaleName,
	  int    cchLocaleName
);

typedef LCID (*P_GetUserDefaultLCID)();

typedef LANGID (*P_GetUserDefaultLangID)();

typedef int (*P_GetUserDefaultGeoName)(
	LPWSTR geoName,
	  int    geoNameCount
);

typedef LANGID (*P_GetSystemDefaultUILanguage)();

typedef int (*P_GetSystemDefaultLocaleName)(
	 LPWSTR lpLocaleName,
	  int    cchLocaleName
);

typedef LCID (*P_GetSystemDefaultLCID)();

typedef LANGID (*P_GetSystemDefaultLangID)();

typedef int (*P_GetLocaleInfoEx)(
	  LPCWSTR lpLocaleName,
	            LCTYPE  LCType,
	 LPWSTR  lpLCData,
	     int     cchData
);

typedef int (*P_GetLocaleInfoA)(
	            LCID   Locale,
	            LCTYPE LCType,
	 LPSTR  lpLCData,
	            int    cchData
);

typedef int (*P_GetLocaleInfoW)(
	            LCID   Locale,
	            LCTYPE LCType,
	 LPWSTR lpLCData,
	            int    cchData
);
//---------------------------------------------------------------------------
// Variables
//---------------------------------------------------------------------------


P_GetCommandLineW				__sys_GetCommandLineW				= NULL;
P_GetCommandLineA				__sys_GetCommandLineA				= NULL;

UNICODE_STRING	Kernel_CommandLineW = { 0 };
ANSI_STRING		Kernel_CommandLineA = { 0 };

P_SetThreadExecutionState		__sys_SetThreadExecutionState		= NULL;
//P_Sleep						__sys_Sleep							= NULL;
P_SleepEx						__sys_SleepEx						= NULL;
P_GetTickCount					__sys_GetTickCount					= NULL;
P_GetTickCount64				__sys_GetTickCount64				= NULL;
P_QueryUnbiasedInterruptTime	__sys_QueryUnbiasedInterruptTime	= NULL;
P_QueryPerformanceCounter		__sys_QueryPerformanceCounter		= NULL;

P_GetUserDefaultUILanguage __sys_GetUserDefaultUILanguage = NULL;
P_GetUserDefaultLocaleName __sys_GetUserDefaultLocaleName = NULL;
P_GetUserDefaultLCID __sys_GetUserDefaultLCID = NULL;
P_GetUserDefaultLangID __sys_GetUserDefaultLangID = NULL;
P_GetUserDefaultGeoName __sys_GetUserDefaultGeoName = NULL;
P_GetSystemDefaultUILanguage __sys_GetSystemDefaultUILanguage = NULL;
P_GetSystemDefaultLocaleName __sys_GetSystemDefaultLocaleName = NULL;
P_GetSystemDefaultLCID __sys_GetSystemDefaultLCID = NULL;
P_GetSystemDefaultLangID __sys_GetSystemDefaultLangID = NULL;
P_GetLocaleInfoEx __sys_GetLocaleInfoEx = NULL;
P_GetLocaleInfoA __sys_GetLocaleInfoA = NULL;
P_GetLocaleInfoW __sys_GetLocaleInfoW = NULL;

//---------------------------------------------------------------------------
// Functions
//---------------------------------------------------------------------------

static LPWSTR Kernel_GetCommandLineW(VOID);

static LPSTR Kernel_GetCommandLineA(VOID);

static EXECUTION_STATE Kernel_SetThreadExecutionState(EXECUTION_STATE esFlags);

static DWORD Kernel_GetTickCount();

static ULONGLONG Kernel_GetTickCount64();

static BOOL Kernel_QueryUnbiasedInterruptTime(PULONGLONG UnbiasedTime);

//static void Kernel_Sleep(DWORD dwMiSecond); // no need hooking sleep as it internally just calls SleepEx

static DWORD Kernel_SleepEx(DWORD dwMiSecond, BOOL bAlert);

static BOOL Kernel_QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);

static LANGID Kernel_GetUserDefaultUILanguage();

static int Kernel_GetUserDefaultLocaleName(
	LPWSTR lpLocaleName,
	int    cchLocaleName
	);

static LCID Kernel_GetUserDefaultLCID();

static LANGID Kernel_GetUserDefaultLangID();

static int Kernel_GetUserDefaultGeoName(
	LPWSTR geoName,
	int    geoNameCount
	);

static LANGID Kernel_GetSystemDefaultUILanguage();

static int Kernel_GetSystemDefaultLocaleName(
	LPWSTR lpLocaleName,
	int    cchLocaleName
	);

static LCID Kernel_GetSystemDefaultLCID();

static LANGID Kernel_GetSystemDefaultLangID();

static int Kernel_GetLocaleInfoEx(
	LPCWSTR lpLocaleName,
	LCTYPE  LCType,
	LPWSTR  lpLCData,
	int     cchData
	);

static int Kernel_GetLocaleInfoA(
	LCID   Locale,
	LCTYPE LCType,
	LPSTR  lpLCData,
	int    cchData
	);

static int Kernel_GetLocaleInfoW(
	LCID   Locale,
	LCTYPE LCType,
	LPWSTR lpLCData,
	int    cchData
	);

//---------------------------------------------------------------------------
// Kernel_Init
//---------------------------------------------------------------------------


_FX BOOLEAN Kernel_Init()
{
	HMODULE module = Dll_Kernel32;

	if (Dll_ImageType == DLL_IMAGE_GOOGLE_CHROME) {

		RTL_USER_PROCESS_PARAMETERS* ProcessParms = Proc_GetRtlUserProcessParameters();

		if (!wcsstr(ProcessParms->CommandLine.Buffer, L" --type=")) { // don't add flags to child processes

			NTSTATUS status;
			WCHAR CustomChromiumFlags[CONF_LINE_LEN];
			status = SbieApi_QueryConfAsIs(NULL, L"CustomChromiumFlags", 0, CustomChromiumFlags, ARRAYSIZE(CustomChromiumFlags));
			if (NT_SUCCESS(status)) {

				const WCHAR* lpCommandLine = ProcessParms->CommandLine.Buffer;
				const WCHAR* lpArguments = SbieDll_FindArgumentEnd(lpCommandLine);
				if (lpArguments == NULL)
					lpArguments = wcsrchr(lpCommandLine, L'\0');

				Kernel_CommandLineW.MaximumLength = ProcessParms->CommandLine.MaximumLength + (CONF_LINE_LEN + 8) * sizeof(WCHAR);
				Kernel_CommandLineW.Buffer = LocalAlloc(LMEM_FIXED,Kernel_CommandLineW.MaximumLength);

				// copy argument 0
				wmemcpy(Kernel_CommandLineW.Buffer, lpCommandLine, lpArguments - lpCommandLine);
				Kernel_CommandLineW.Buffer[lpArguments - lpCommandLine] = 0;
				
				// add custom arguments
				if(Kernel_CommandLineW.Buffer[lpArguments - lpCommandLine - 1] != L' ')
					wcscat(Kernel_CommandLineW.Buffer, L" ");
				wcscat(Kernel_CommandLineW.Buffer, CustomChromiumFlags);

				// add remaining arguments
				wcscat(Kernel_CommandLineW.Buffer, lpArguments);


				Kernel_CommandLineW.Length = wcslen(Kernel_CommandLineW.Buffer) * sizeof(WCHAR);

				RtlUnicodeStringToAnsiString(&Kernel_CommandLineA, &Kernel_CommandLineW, TRUE);

				void* GetCommandLineW = GetProcAddress(Dll_KernelBase ? Dll_KernelBase : Dll_Kernel32, "GetCommandLineW");
				SBIEDLL_HOOK(Kernel_, GetCommandLineW);

				void* GetCommandLineA = GetProcAddress(Dll_KernelBase ? Dll_KernelBase : Dll_Kernel32, "GetCommandLineA");
				SBIEDLL_HOOK(Kernel_, GetCommandLineA);
			}
		}
	}

	if (SbieApi_QueryConfBool(NULL, L"BlockInterferePower", FALSE)) {

        SBIEDLL_HOOK(Kernel_, SetThreadExecutionState);
    }

	if (SbieApi_QueryConfBool(NULL, L"UseChangeSpeed", FALSE)) {

		SBIEDLL_HOOK(Kernel_, GetTickCount);
		P_GetTickCount64 GetTickCount64 = Ldr_GetProcAddrNew(Dll_Kernel32, L"GetTickCount64", "GetTickCount64");
		if (GetTickCount64) {
			SBIEDLL_HOOK(Kernel_, GetTickCount64);
		}
		P_QueryUnbiasedInterruptTime QueryUnbiasedInterruptTime = Ldr_GetProcAddrNew(Dll_Kernel32, L"QueryUnbiasedInterruptTime", "QueryUnbiasedInterruptTime");
		if (QueryUnbiasedInterruptTime) {
			SBIEDLL_HOOK(Kernel_, QueryUnbiasedInterruptTime);
		}
		SBIEDLL_HOOK(Kernel_, QueryPerformanceCounter);
		//SBIEDLL_HOOK(Kernel_, Sleep);
		SBIEDLL_HOOK(Kernel_, SleepEx);	
	}
	if (SbieApi_QueryConfBool(NULL, L"UseSpoofLocale", FALSE))
	{
		SBIEDLL_HOOK(Kernel_, GetUserDefaultUILanguage);
		//P_GetUserDefaultLocaleName GetUserDefaultLocaleName = Ldr_GetProcAddrNew(Dll_Kernel32, L"GetUserDefaultLocaleName", "GetUserDefaultLocaleName");
		//if (GetUserDefaultLocaleName)
		//	SBIEDLL_HOOK(Kernel_, GetUserDefaultLocaleName);
		SBIEDLL_HOOK(Kernel_, GetUserDefaultLCID);
		SBIEDLL_HOOK(Kernel_, GetUserDefaultLangID);
		//SBIEDLL_HOOK(Kernel_, GetUserDefaultGeoName);
		SBIEDLL_HOOK(Kernel_, GetSystemDefaultUILanguage);
		//P_GetSystemDefaultLocaleName GetSystemDefaultLocaleName = Ldr_GetProcAddrNew(Dll_Kernel32, L"GetSystemDefaultLocaleName", "GetSystemDefaultLocaleName");
		//if (GetSystemDefaultLocaleName)
		//	SBIEDLL_HOOK(Kernel_, GetSystemDefaultLocaleName);
		SBIEDLL_HOOK(Kernel_, GetSystemDefaultLCID);
		SBIEDLL_HOOK(Kernel_, GetSystemDefaultLangID);
		

		/*SBIEDLL_HOOK(Kernel_, GetLocaleInfoA);
		SBIEDLL_HOOK(Kernel_, GetLocaleInfoW);
		P_GetLocaleInfoEx GetLocaleInfoEx = Ldr_GetProcAddrNew(Dll_Kernel32, L"GetLocaleInfoEx", "GetLocaleInfoEx");
		if (GetLocaleInfoEx)
			SBIEDLL_HOOK(Kernel_, GetLocaleInfoEx);*/
	}

	return TRUE;
}


//---------------------------------------------------------------------------
// Kernel_GetCommandLineW
//---------------------------------------------------------------------------


_FX LPWSTR Kernel_GetCommandLineW(VOID)
{
	return Kernel_CommandLineW.Buffer;
	//return __sys_GetCommandLineW();
}


//---------------------------------------------------------------------------
// Kernel_GetCommandLineA
//---------------------------------------------------------------------------


_FX LPSTR Kernel_GetCommandLineA(VOID)
{
	return Kernel_CommandLineA.Buffer;
	//return __sys_GetCommandLineA();
}


//---------------------------------------------------------------------------
// Kernel_SetThreadExecutionState
//---------------------------------------------------------------------------


_FX EXECUTION_STATE Kernel_SetThreadExecutionState(EXECUTION_STATE esFlags) 
{
	SetLastError(ERROR_ACCESS_DENIED);
	return 0;
	//return __sys_SetThreadExecutionState(esFlags);
}


//---------------------------------------------------------------------------
// Kernel_GetTickCount
//---------------------------------------------------------------------------


_FX DWORD Kernel_GetTickCount() 
{
	ULONG add = SbieApi_QueryConfNumber(NULL, L"AddTickSpeed", 1);
	ULONG low = SbieApi_QueryConfNumber(NULL, L"LowTickSpeed", 1);
	if (low != 0)
		return __sys_GetTickCount() * add / low;
	return __sys_GetTickCount() * add;
}


//---------------------------------------------------------------------------
// Kernel_GetTickCount64
//---------------------------------------------------------------------------


_FX ULONGLONG Kernel_GetTickCount64() 
{
	ULONG add = SbieApi_QueryConfNumber(NULL, L"AddTickSpeed", 1);
	ULONG low = SbieApi_QueryConfNumber(NULL, L"LowTickSpeed", 1);
	if (low != 0)
		return __sys_GetTickCount64() * add / low;
	return __sys_GetTickCount64() * add;
}


//---------------------------------------------------------------------------
// Kernel_QueryUnbiasedInterruptTime
//---------------------------------------------------------------------------


_FX BOOL Kernel_QueryUnbiasedInterruptTime(PULONGLONG UnbiasedTime)
{
	BOOL rtn = __sys_QueryUnbiasedInterruptTime(UnbiasedTime);
	ULONG add = SbieApi_QueryConfNumber(NULL, L"AddTickSpeed", 1);
	ULONG low = SbieApi_QueryConfNumber(NULL, L"LowTickSpeed", 1);
	if (low != 0)
		*UnbiasedTime *= add / low;
	else
		*UnbiasedTime *= add;
	return rtn;
}


//---------------------------------------------------------------------------
// Kernel_SleepEx
//---------------------------------------------------------------------------


_FX DWORD Kernel_SleepEx(DWORD dwMiSecond, BOOL bAlert) 
{
	ULONG add = SbieApi_QueryConfNumber(NULL, L"AddSleepSpeed", 1);
	ULONG low = SbieApi_QueryConfNumber(NULL, L"LowSleepSpeed", 1);
	if (add != 0 && low != 0)
		return __sys_SleepEx(dwMiSecond * add / low, bAlert);
	return __sys_SleepEx(dwMiSecond, bAlert);
}


//---------------------------------------------------------------------------
// Kernel_QueryPerformanceCounter
//---------------------------------------------------------------------------


_FX BOOL Kernel_QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount)
{
	BOOL rtn = __sys_QueryPerformanceCounter(lpPerformanceCount);
	ULONG add = SbieApi_QueryConfNumber(NULL, L"AddTickSpeed", 1);
	ULONG low = SbieApi_QueryConfNumber(NULL, L"LowTickSpeed", 1);
	if (add != 0 && low != 0)
		lpPerformanceCount->QuadPart = lpPerformanceCount->QuadPart * add / low;
	return rtn;
}

_FX LANGID Kernel_GetUserDefaultUILanguage() {
	return (LANGID)SbieApi_QueryConfNumber(NULL, L"FalseLCID", 1033);
}
typedef int (*LCIDToLocaleName)(
	            LCID   Locale,
				LPWSTR lpName,
	            int    cchName,
	            DWORD  dwFlags
);
_FX int Kernel_GetUserDefaultLocaleName(
	LPWSTR lpLocaleName,
	int    cchLocaleName
) {
	LCIDToLocaleName ltln = Ldr_GetProcAddrNew(Dll_Kernel32, L"LCIDToLocaleName", "LCIDToLocaleName");
	if (ltln) {
		return ltln(SbieApi_QueryConfNumber(NULL, L"FalseLCID", 1033), lpLocaleName, cchLocaleName, 0);
	}
	else {
		if (cchLocaleName >= 4)
			lstrcpy(lpLocaleName, L"en_US");
		else
			return 0;
	}
	return 4;
}

_FX LCID Kernel_GetUserDefaultLCID() {
	return SbieApi_QueryConfNumber(NULL, L"FalseLCID", 1033);
}

_FX LANGID Kernel_GetUserDefaultLangID() {
	return (LANGID)SbieApi_QueryConfNumber(NULL, L"FalseLCID", 1033);
}
char* itoa2(int num, char* str, int radix)
{
	char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	unsigned unum;
	int i = 0, j, k;

	
	if (radix == 10 && num < 0)
	{
		unum = (unsigned)-num;
		str[i++] = '-';
	}
	else unum = (unsigned)num;

	
	do
	{
		str[i++] = index[unum % (unsigned)radix];
		unum /= radix;

	} while (unum);

	str[i] = '\0';

	
	if (str[0] == '-') k = 1;
	else k = 0;

	char temp;
	for (j = k; j <= (i - 1) / 2; j++)
	{
		temp = str[j];
		str[j] = str[i - 1 + k - j];
		str[i - 1 + k - j] = temp;
	}

	return str;

}

wchar_t* GetWC(const char* c)
{
	const size_t cSize = strlen(c) + 1;
	
	wchar_t* wc=(wchar_t*)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, sizeof(wchar_t) * cSize);
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, c, cSize, wc, cSize);
	return wc;
}
_FX int Kernel_GetUserDefaultGeoName(
	LPWSTR geoName,
	int    geoNameCount
) {
	char* buf = (char*)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, sizeof(char) * geoNameCount);
	itoa2(SbieApi_QueryConfNumber(NULL, L"FalseAreaNumber", 840),buf,10);
	wchar_t* tmp = GetWC(buf);
	int length = sizeof(GetWC(buf));
	lstrcpy(geoName, tmp);
	GlobalFree(buf);
	GlobalFree(tmp);
	return length;
}

_FX LANGID Kernel_GetSystemDefaultUILanguage() {
	return (LANGID)SbieApi_QueryConfNumber(NULL, L"FalseLCID", 1033);
}

_FX int Kernel_GetSystemDefaultLocaleName(
	LPWSTR lpLocaleName,
	int    cchLocaleName
) {
	LCIDToLocaleName ltln = Ldr_GetProcAddrNew(Dll_Kernel32, L"LCIDToLocaleName", "LCIDToLocaleName");
	if (ltln) {
		return ltln(SbieApi_QueryConfNumber(NULL, L"FalseLCID", 1033), lpLocaleName, cchLocaleName, 0);
	}
	else {
		if (cchLocaleName >= 4)
			lstrcpy(lpLocaleName, L"en_US");
		else
			return 0;
	}
	return 4;
}

_FX LCID Kernel_GetSystemDefaultLCID() {
	return SbieApi_QueryConfNumber(NULL, L"FalseLCID", 1033);
}

_FX LANGID Kernel_GetSystemDefaultLangID() {
	return (LANGID)SbieApi_QueryConfNumber(NULL, L"FalseLCID", 1033);
}

/*_FX int Kernel_GetLocaleInfoEx(
	LPCWSTR lpLocaleName,
	LCTYPE  LCType,
	LPWSTR  lpLCData,
	int     cchData
);

_FX int Kernel_GetLocaleInfoA(
	LCID   Locale,
	LCTYPE LCType,
	LPSTR  lpLCData,
	int    cchData
);

_FX int Kernel_GetLocaleInfoW(
	LCID   Locale,
	LCTYPE LCType,
	LPWSTR lpLCData,
	int    cchData
);
*/
