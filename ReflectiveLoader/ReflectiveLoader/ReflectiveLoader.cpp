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
	printf("[!] Please give the file path to the intended DLL to be injected and the name of the process to inject into. Examples:\n");
	printf("[!] ReflectiveLoader.exe -d C:\\ACHacks.dll -p ac_client.exe\n");
	return;
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
	}
	//make sure that the variables are not 0.
	if (!(procName.size() > 0) || !(dllPath.size() > 0))
	{
		help();
		return -1;
	}

	//attempt to find the target process ID.
	printf("[*] Attempting to find Process ID of the Process [%s].\n", procName.c_str());
	
	DWORD procId = getProcId(procName);
	
	//make sure the process Id exists.
	if (!procId)
	{
		printf("[!] ProcId is NULL, ensure that the Process [%s] is running.\n", procName.c_str());
		printf("[!] Exiting.\n");
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
	//start the manual mapping proceess using the target dll for injection and the handle to the process.
	ManualMap(hProc, dllPath.c_str());

}