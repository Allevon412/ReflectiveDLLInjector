#pragma once

#include <vector>
#include <windows.h>
#include <TlHelp32.h>
#include <string>

DWORD getProcId(std::string procName);

uintptr_t GetModuleBaseAddr(DWORD procId, std::string modName);

uintptr_t findDMAAddy(HANDLE hProc, uintptr_t pt, std::vector<unsigned int> offsets);