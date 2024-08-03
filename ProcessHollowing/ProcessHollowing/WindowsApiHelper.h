#pragma once

typedef LONG NTSTATUS;

#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)

typedef NTSTATUS(NTAPI* pNtGetContextThread)(HANDLE hThread, PCONTEXT lpContext);
typedef NTSTATUS(NTAPI* pNtResumeThread)(HANDLE ThreadHandle, PULONG SuspendCount);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten);

pNtGetContextThread NtGetContextThread = NULL;
pNtResumeThread NtResumeThread = NULL;
pNtWriteVirtualMemory NtWriteVirtualMemory = NULL;

void InitializeNtFunctions()
{
	HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");

	if (hNtDll)
	{
		NtGetContextThread = (pNtGetContextThread)GetProcAddress(hNtDll, "NtGetContextThread");
		NtResumeThread = (pNtResumeThread)GetProcAddress(hNtDll, "NtResumeThread");
		NtWriteVirtualMemory = (pNtWriteVirtualMemory)GetProcAddress(hNtDll, "NtWriteVirtualMemory");
	}
}

NTSTATUS MyNtGetContextThread(HANDLE hThread, PCONTEXT lpContext)
{
	if (NtGetContextThread)
	{
		return NtGetContextThread(hThread, lpContext);
	}

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS MyNtResumeThread(HANDLE ThreadHandle, PULONG SuspendCount)
{
	if (NtResumeThread)
	{
		return NtResumeThread(ThreadHandle, SuspendCount);
	}

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS MyNtWriteVirtualMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten)
{
	if (NtWriteVirtualMemory)
	{
		return NtWriteVirtualMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten);
	}

	return STATUS_UNSUCCESSFUL;
}