#pragma once

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <TlHelp32.h>

//declare windows API prototypes that will be passed to our loaderstub.
typedef HINSTANCE(WINAPI* f_LoadLibraryA)(const char* lpLibFileName);
typedef UINT_PTR(WINAPI* f_GetProcAddress)(HINSTANCE hModule, const char* lpProcName);
using f_DLL_ENTRY_POINT = BOOL(WINAPI*)(void* hDll, DWORD dwReason, void* pReserved);
using f_OpenProcess = HANDLE(WINAPI*)(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
using f_VirtualALlocEx = LPVOID(WINAPI*)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);

//declare structure that will be passed to our loader stub that contains necessary information for loading.
struct MANUAL_MAPPING_DATA
{
	f_LoadLibraryA pLoadLibraryA;
	f_GetProcAddress pGetProcAddress;
	HINSTANCE hMod;
};

//delare functions.
//perform memory mapping manually in remote process.
BOOL ManualMap(HANDLE hProc, const char* szDllFile);

//performs relocations and loading of dll into procecss from within the process itself.
void __stdcall LoaderStub(BYTE* imageBase);