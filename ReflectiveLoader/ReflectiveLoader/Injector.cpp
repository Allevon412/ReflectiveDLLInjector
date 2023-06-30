#include "Injector.h"

//function delcaration.
BOOL ManualMap(HANDLE hProc, const char* szDllFile)
{
	//declare necessary variables.
	BYTE* pSrcData = nullptr;
	IMAGE_NT_HEADERS* pOldNtHeader = nullptr;
	IMAGE_OPTIONAL_HEADER* pOldOptHeader = nullptr;
	IMAGE_FILE_HEADER* pOldFileHeader = nullptr;
	BYTE* pTargetBase = nullptr;

	//ensure that our file exists on disc.
	if (GetFileAttributesA(szDllFile) == INVALID_FILE_ATTRIBUTES)
	{
		printf("[!] File doesn't exist\n");
		return false;
	}
	printf("[*] Attempting to open file handle\n");
	std::ifstream File(szDllFile, std::ios::binary | std::ios::ate);

	//ensure that we are able to open a file handle.
	if (File.fail())
	{
		printf("[!] Opening the file failed: [%X]\n", (DWORD)File.rdstate());
		File.close();
		return false;
	}
	
	//ensure that our file has a valid size.
	auto FileSize = File.tellg();
	if (FileSize < 0x1000)
	{
		printf("[!] Filesize is invalid.\n");
		File.close();
		return false;
	}

	//allocate memory for our file using the size as a measure.
	printf("[*] Allocating memory for the dll.\n");
	pSrcData = new BYTE[static_cast<UINT_PTR>(FileSize)];
	if (!pSrcData)
	{
		printf("[!] Memory Allocation Failed\n");
		File.close();
		return false;
	}

	//read the file into our allocated buffer.
	printf("[*] Memory allocated at location: [0x%p], attempting to read file contents into memory\n", pSrcData);
	File.seekg(0, std::ios::beg);
	File.read(reinterpret_cast<char*>(pSrcData), FileSize);
	File.close();

	//ensure that our file is in the pe format.
	if (reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData)->e_magic != 0x5A4D)
	{
		printf("[!] File is not a valid PE file Magic Bytes do not equal \"MZ\"\n");
		delete[] pSrcData;
		return false;
	}
	
	//create pointers to the PE Optional Header, NtHeaders, and FileHeaders for mapping usage.
	pOldNtHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(pSrcData + reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData)->e_lfanew);
	pOldOptHeader = &pOldNtHeader->OptionalHeader;
	pOldFileHeader = &pOldNtHeader->FileHeader;

#ifdef _WIN64
	if (pOldFileHeader->Machine != IMAGE_FILE_MACHINE_AMD64)
	{
		printf("[!] Invalid Platform in target DLL: [%x]\n", pOldFileHeader->Machine);
		delete[] pSrcData;
		return false;
	}
#else
	if (pOldFileHeader->Machine != IMAGE_FILE_MACHINE_I386)
	{
		printf("[!] Invalid Platform in target DLL: [%x]\n", pOldFileHeader->Machine);
		delete[] pSrcData;
		return false;
	}

#endif
	//create a map data structure that will be passed to our LoaderStub.
	MANUAL_MAPPING_DATA mData{ 0 };
	mData.pLoadLibraryA = LoadLibraryA;
	mData.pGetProcAddress = reinterpret_cast<f_GetProcAddress>(GetProcAddress);


	//allocate memory in the target process the size of the image plus the size of the mapping data which will be passed to our loader stub.
	printf("[*] Allocating memory in the size of: [0x%x] in remote process.\n", pOldOptHeader->SizeOfImage + sizeof(mData));
	pTargetBase = reinterpret_cast<BYTE*>(VirtualAllocEx(hProc, nullptr, pOldOptHeader->SizeOfImage + sizeof(mData), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

	//if the memory allocation failed.
	if (!pTargetBase)
	{
		printf("[!] Memory Allocation Failed [0x%X]\n", GetLastError());
		delete[] pSrcData;
		return false;
	}
	printf("[*] Memory Allocation successful. Memory located at: [0x%p]\n", pTargetBase);
	
	//next we write the headers of our dll into the remote process.
	printf("[*] Writing the Headers of our DLL into target process\n");
	if (!WriteProcessMemory(hProc, pTargetBase, pSrcData, pOldOptHeader->SizeOfHeaders, nullptr))
	{
		printf("[!] Error Mapping the Headers into memory please read error code: [0x%X]\n", GetLastError());
		return false;
	}

	//write our sections into the remote process.
	auto* pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
	
	//for each section write it into the virtual address of our allocated memory in the remote process using the raw data from our dll file.
	printf("[*] Attempting to write our sections into the remote process\n");
	for (UINT i = 0; i != pOldFileHeader->NumberOfSections; i++, pSectionHeader++)
	{
		if (pSectionHeader->SizeOfRawData)
		{
			printf("%5s%s%s%p%s", "[*] Attempting to write section: [", pSectionHeader->Name, "] at location: [0x", pTargetBase + pSectionHeader->VirtualAddress, "]\n");
			if (!WriteProcessMemory(hProc, pTargetBase + pSectionHeader->VirtualAddress, pSrcData + pSectionHeader->PointerToRawData, pSectionHeader->SizeOfRawData, nullptr))
			{
				printf("[!] Error Mapping the sections error code [0x%X]\n", GetLastError());
				delete[] pSrcData;
				VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
				return false;
			}
		}
	}
	
	printf("[*] Sections written into memory.\n[*]Attempting to write our mapping data at end of image location: [0x%p]\n", pTargetBase + pOldOptHeader->SizeOfImage);
	WriteProcessMemory(hProc, pTargetBase + pOldOptHeader->SizeOfImage, &mData, sizeof(mData), nullptr);


	DWORD ImageSize = pOldOptHeader->SizeOfImage;
	delete[] pSrcData;

	//allocate memory in our remote process for our loader stub.
	printf("[*] Attemtping to allocate memory in the remote process for our Loader Stub.\n");
	void* pLoaderStub = VirtualAllocEx(hProc, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!pLoaderStub)
	{
		printf("[!] Loader Memory Allocation Failed [0x%X]\n", GetLastError());
		free(pLoaderStub);
		return false;
	}
	
	printf("[*] Allocation successful. Loaderstub memory location: [0x%p]\n[*] Attempting to Write LoaderStub\n", pLoaderStub);
	//write our loader stub into remote process' memory.
	WriteProcessMemory(hProc, pLoaderStub, LoaderStub, 0x1000, nullptr);

	//create thread in remote process that will start the loader using our DLL and the map data structure as an argumnet.
	printf("[*] Creating Thread in remote process that will start the loading routine\n");
	HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoaderStub), pTargetBase, 0, NULL);
	if (!hThread)
	{
		printf("[!] Thread Creation failed exiting: [0x%x]", GetLastError());
		VirtualFreeEx(hProc, pLoaderStub, 0, MEM_RELEASE);
		VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
		return false;
	}
	printf("%5s", "[*] Checking for completion of the loading routine\n");
	HINSTANCE hCheck = NULL;
	while (!hCheck)
	{
		MANUAL_MAPPING_DATA mapData = { 0 };
		ReadProcessMemory(hProc, pTargetBase + ImageSize, &mapData, sizeof(mapData), nullptr);
		hCheck = mapData.hMod;
		Sleep(10);
	}

	printf("[*] Loading routine successfully completed. Cleaning up!\n");
	CloseHandle(hThread);
}

#define RELOC_FLAG32(relInfo) ((relInfo >> 0xC) == IMAGE_REL_BASED_HIGHLOW)
#define RELOC_FLAG64(relInfo) ((relInfo >> 0xC) == IMAGE_REL_BASED_DIR64)

#ifdef _WIN64
#define RELOC_FLAG RELOC_FLAG64
#else
#define RELOC_FLAG RELOC_FLAG32
#endif

void __stdcall LoaderStub(BYTE* imageBase)
{
	if (!imageBase)
	{
		return;
	}

	//obtain pointers to the base address, optional header, and DLLMain function using the library mapped in memory as our argument.
	BYTE* pBase = imageBase;
	auto* pOpt = &reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + reinterpret_cast<IMAGE_DOS_HEADER*>(imageBase)->e_lfanew)->OptionalHeader;
	auto _DllMain = reinterpret_cast<f_DLL_ENTRY_POINT>(pBase + pOpt->AddressOfEntryPoint);

	//create a pointer to our mapping data by offsetting the size of the image.
	MANUAL_MAPPING_DATA* pMapData = reinterpret_cast<MANUAL_MAPPING_DATA*>(imageBase + pOpt->SizeOfImage);

	//obtain pointers to tthe LoadLibrary and GetProcAddress functions.
	auto _LoadLibraryA = pMapData->pLoadLibraryA;
	auto _GetProcAddress = pMapData->pGetProcAddress;

	//obtain Relocation Delta.
	BYTE* LocationDelta = pBase - pOpt->ImageBase;
	if (LocationDelta)
	{
		//ensure there is entries in the relocation table. If not just continue.
		if (!pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size)
		{
			return;
		}
		
		//obtain a pointer to the relocation data. Using our mapped image + offset of relocation table's virtual address.
		auto* pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

		//while there are entires in the relocation table:
		while (pRelocData->VirtualAddress)
		{
			//obtain amount of entries for this part of the table.
			UINT AmountOfEntries = (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			WORD* pRelativeInfo = reinterpret_cast<WORD*>(pRelocData + 1); //obtain the relocation type.
			for (UINT i = 0; i != AmountOfEntries; i++, pRelativeInfo++)
			{
				if (RELOC_FLAG(*pRelativeInfo))
				{
					//obtain the virtual address for our relocation entry.
					UINT_PTR* pPatch = reinterpret_cast<UINT_PTR*>(pBase + pRelocData->VirtualAddress + ((*pRelativeInfo) & 0xFFF));
					//add the delta to relocate it to the correct location of the module mapped in the current memory space.
					*pPatch += reinterpret_cast<UINT_PTR>(LocationDelta);
				}
			}
			//obtain next entry in the table.
			pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(pRelocData) + pRelocData->SizeOfBlock);
		}
	}

	//obtain entries from the import table/
	if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size)
	{
		//obtain the virtual address of the importDescriptor table.
		auto* pImportDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*> (pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
		while (pImportDescriptor->Name)
		{
			//setup pointers to the imported library / function name.
			char* szMod = reinterpret_cast<char*>(pBase + pImportDescriptor->Name);
			HINSTANCE hDLL = _LoadLibraryA(szMod);
			ULONG_PTR* pThunkRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescriptor->OriginalFirstThunk);
			ULONG_PTR* pFuncRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescriptor->FirstThunk);

			//if the First thunk is null use the next one.
			if (!pThunkRef)
				pThunkRef = pFuncRef;
			
			//loop through each function.
			for (; *pThunkRef; ++pThunkRef, ++pFuncRef)
			{
				//GetProceAddress(lib, "ReadProcessMemory"); name
				//GetProcAddress(lib, (char*)42); ordinal

				//import the functions by ordinal or name.
				if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef))
				{
					*pFuncRef = _GetProcAddress(hDLL, reinterpret_cast<char*>(*pThunkRef & 0xFFFF));
				}
				else
				{
					auto pImport = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + (*pThunkRef));
					*pFuncRef = _GetProcAddress(hDLL, pImport->Name);
				}
			}
			//obtain the next descriptor entry.
			++pImportDescriptor;
		}
	}

	//find the callback routines and attach them.
	if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size)
	{
		auto* pTLS = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
		auto* pCallBack = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTLS->AddressOfCallBacks);
		for (; pCallBack && *pCallBack; ++pCallBack)
		{
			(*pCallBack)(pBase, DLL_PROCESS_ATTACH, nullptr); 
		}
	}

	//execute our DLL by calling the entrypoint address with the argument of DLL_PROCESS_ATTACH.

	//TODO: Write Code that will accept arguments and pass them to the DLLMain function.
	_DllMain(pBase, DLL_PROCESS_ATTACH, nullptr);

	//set our hMod value in our map data so that we can read it from our original process and determine that DLL has been executed properly.
	pMapData->hMod = reinterpret_cast<HINSTANCE>(pBase);
}