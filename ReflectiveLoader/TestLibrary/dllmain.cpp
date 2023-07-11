// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <iostream>


void go() {

    AllocConsole();

    // Get the handle to the console window
    HWND hwndConsole = GetConsoleWindow();

    // You can modify the console window properties if needed
    // For example, to set the window title:
    SetConsoleTitle(L"My Console");

    // Retrieve the standard input/output handles
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

    // Redirect the standard input/output handles to the console window
    SetStdHandle(STD_INPUT_HANDLE, hStdin);
    SetStdHandle(STD_OUTPUT_HANDLE, hStdout);

    // You can now use standard input/output functions like cout, cin, etc.

    // Example: Print a message to the console
    const wchar_t* message = L"[*] Module is currently loaded at address: [0x%p]\n";
    wchar_t buffer[256];

    HMODULE hModule = GetModuleHandle(nullptr);
    swprintf(buffer, sizeof(buffer) / sizeof(wchar_t), message, reinterpret_cast<uintptr_t>(hModule));
    DWORD bytesWritten;
    WriteConsole(hStdout, buffer, wcslen(buffer), &bytesWritten, nullptr);
    const wchar_t* message2 = L"[*] In the test library. Doing a Test Print!\n";
    WriteConsole(hStdout, message2, wcslen(message2), &bytesWritten, nullptr);

    wchar_t buffer2[256];
    DWORD charsRead;
    ReadConsole(hStdin, buffer2, 1, &charsRead, nullptr);

    FreeConsole();
    return;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        go();
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

