// ReflectiveLoader.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <locale>
#include <codecvt>


#include "process.h"
#include "Injector.h"


//semi funcitonal help menu print function.
void help()
{
	printf("[!] Please give the file path to the intended DLL to be injected, the name of the process to inject into, and type of injection in numerical form. Examples:\n");
	printf("[!] ReflectiveLoader.exe -d C:\\ACHacks.dll -p ac_client.exe -t 1 | 2\n");
	return;
}


//The injector should check to ensure the process is running.
DWORD ensureACisRunning(const char* procName)
{
	DWORD procId = getProcId(procName);
	if (procId)
	{
		// assault cube is already running no need to start it.
		return procId;
	}
	else
	{
		STARTUPINFOA info = { sizeof(info) };
		PROCESS_INFORMATION processInfo;

		//start assault cube process becuase I am too lazy to do it myself.
		if (CreateProcessA("C:\\Program Files (x86)\\AssaultCube\\assaultcube.bat", NULL, NULL, NULL, FALSE, 0, NULL, "C:\\Program Files (x86)\\AssaultCube\\", &info, &processInfo))
		{
			WaitForSingleObject(processInfo.hProcess, INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}
		//if we cant create the process.
		else
		{
			return 0;
		}
		return processInfo.dwProcessId;
	}
}

int main(int argc, char* argv[])
{
	// check for corret number of arguments
	if (argc == 1)
	{
		help();
		return -1;
	}
	//declare variables for storing our processname and dll path.
	std::string procName;
	std::string dllPath;
	std::string type;

	//loop through all command line arguments.
	for (int i = 1; i < argc; i++)
	{
		//if the command line argument matches any of the following.
		char* arg = argv[i];
		if (!strcmp(arg, "-p"))
		{
			//set variable equal to the command line switch + 1;
			procName = argv[i + 1];

		}
		if (!strcmp(arg, "-d"))
		{
			dllPath = argv[i + 1];
		}
		if (!strcmp(arg, "-h"))
		{
			help();
			return -1;
		}
		if (!strcmp(arg, "-t"))
		{
			type = argv[i + 1];
		}
	}
	
	//make sure that the variables are not 0.
	if (!(procName.size() > 0) || !(dllPath.size() > 0))
	{
		help();
		return -1;
	}

	//attempt to find the target process ID.
	printf("[*] Attempting to find Process ID of the Process [%s].\n", procName.c_str());
	
	
	DWORD procId = ensureACisRunning(procName.c_str());
	//make sure the process Id exists.
	if (!procId)
	{
		printf("[!] ProcId is NULL, ensure that the Process [%s] is running.\n", procName.c_str());
		printf("[!] Also may have failed to launch assault cube.\n");
		return -1;
	}

	//attmept to open a handle to the process and find the base address.
	printf("[*] Process ID found: [%d]!\n", procId);
	

	//open handle to the remote process.
	printf("[*] Opening a Handle to the Process!\n");
	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, NULL, procId);

	//ensure that the process handle is valid.
	if (hProc == INVALID_HANDLE_VALUE)
	{
		printf("[!] Error opening handle to Process [%s].\n", procName.c_str());
		printf("[!] Please ensure that the current program is running with Administrator privileges before continuing.\n");
		printf("[!] Error code is: 0x%X \n", GetLastError());
		return -1;

	}
	if (!strcmp(type.c_str(), "1"))
	{
		//start the manual mapping proceess using the target dll for injection and the handle to the process.
		ManualMap(hProc, dllPath.c_str());
	}
	else
	{
		printf("[*] Performing normal DLL injection via LoadLibraryA call.\n");
		printf("\t[*] This is normally only done for debugging purposes.\n");

		if (hProc && hProc != INVALID_HANDLE_VALUE)
		{
			
			void* loc = VirtualAllocEx(hProc, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

			printf("[*] Virtual Memeory Allocated at location: %p\n", loc);

			WriteProcessMemory(hProc, loc, dllPath.c_str(), dllPath.length() + 1, 0);

			printf("[*] DLL Path [%s] written to location: %p\n", dllPath, loc);

			printf("[*] Using CreateRemoteThread to start a LoadLibraryA function call\n");

			HANDLE hThread = CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, loc, 0, 0);

			if (hThread)
			{
				CloseHandle(hThread);
			}
			printf("[*] Success, closing thread handle and exiting.\n");
		}

		if (hProc)
		{
			CloseHandle(hProc);
		}
		return 0;
	}

}